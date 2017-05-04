/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_MYSQLROUTING_INCLUDED
#define ROUTING_MYSQLROUTING_INCLUDED

/** @file
 * @brief Defining the class MySQLRouting
 *
 * This file defines the main class `MySQLRouting` which is used to configure,
 * start and manage a connection routing from clients and MySQL servers.
 *
 */

#include "config.h"
#include "destination.h"
#include "filesystem.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/mysql_protocol.h"
#include "plugin_config.h"
#include "utils.h"
#include "mysqlrouter/routing.h"

#include <array>
#include <atomic>
#include <iostream>
#include <map>
#include <memory>

#ifndef _WIN32
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#  include <unistd.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif
#include <curl/curl.h>


#ifdef _WIN32
#  ifdef routing_DEFINE_STATIC
#    define ROUTING_API
#  else
#    ifdef routing_EXPORTS
#      define ROUTING_API __declspec(dllexport)
#    else
#      define ROUTING_API __declspec(dllimport)
#    endif
#  endif
#else
#  define ROUTING_API
#endif

using std::string;
using mysqlrouter::URI;

/** @class MySQLRoutering
 *  @brief Manage Connections from clients to MySQL servers
 *
 *  The class MySQLRouter is used to start a service listening on a particular
 *  TCP port for incoming MySQL Client connection and route these to a MySQL
 *  Server.
 *
 *  Connection routing will not analyze or parse any MySQL package nor will
 *  it do any authentication. It will not handle errors from the MySQL server
 *  and not automatically recover. The client communicate through MySQL Router
 *  just like it would directly connecting.
 *
 *  The MySQL Server is chosen from a given list of hosts or IP addresses
 *  (with or without TCP port) based on the the mode. For example, mode
 *  read-only will go through the list of servers in a round-robin way. The
 *  mode read-write will always go through the list from the beginning and
 *  failover to the next available.
 *
 *
 *  Example usage: bind to all IP addresses and use TCP Port 7001
 *
 *   MySQLRouting r(routing::AccessMode::kReadWrite, "0.0.0.0", 7001);
 *   r.destination_connect_timeout = 1;
 *   r.set_destinations_from_csv("10.0.10.5;10.0.11.6");
 *   r.start();
 *
 *  The above example will, when MySQL running on 10.0.10.5 is not available,
 *  use 10.0.11.6 to setup the connection routing.
 *
 */
class MySQLRouting {
public:
  /** @brief Default constructor
   *
   * @param port TCP port for listening for incoming connections
   * @param optional bind_address bind_address Bind to particular IP address
   * @param optional route Name of connection routing (can be empty string)
   * @param optional max_connections Maximum allowed active connections
   * @param optional destination_connect_timeout Timeout trying to connect destination server
   * @param optional max_connect_errors Maximum connect or handshake errors per host
   * @param optional connect_timeout Timeout waiting for handshake response
   * @param optional socket_operations object handling the operations on network sockets
   */
  MySQLRouting(routing::AccessMode mode, uint16_t port,
               const string &bind_address = string{"0.0.0.0"},
               const string &route_name = string{},
               int max_connections = routing::kDefaultMaxConnections,
               int destination_connect_timeout = routing::kDefaultDestinationConnectionTimeout,
               unsigned long long max_connect_errors = routing::kDefaultMaxConnectErrors,
               unsigned int connect_timeout = routing::kDefaultClientConnectTimeout,
               unsigned int net_buffer_length = routing::kDefaultNetBufferLength,
               routing::SocketOperationsBase *socket_operations = routing::SocketOperations::instance());

  /** @brief Starts the service and accept incoming connections
   *
   * Starts the connection routing service and start accepting incoming
   * MySQL client connections. Each connection will be further handled
   * in a separate thread.
   *
   * Throws std::runtime_error on errors.
   *
   */
  void start();

  /** @brief Asks the service to stop
   *
   */
  void stop();

  /** @brief Returns whether the service is stopping
   *
   * @return a bool
   */
  bool stopping() {
    return stopping_.load();
  }

  /** @brief Sets the destinations from URI
   *
   * Sets destinations using the given string and the given mode. The string
   * should be a comma separated list of MySQL servers.
   *
   * The mode is one of MySQLRouting::Mode, for example MySQLRouting::Mode::kReadOnly.
   *
   * Example of destinations:
   *   "10.0.10.5,10.0.11.6:3307"
   *
   * @param csv destinations as comma-separated-values
   */
  void set_destinations_from_csv(const string &csv);

  void set_destinations_from_uri(const URI &uri);

  /* set abac source control information */
  void set_abac_service(const string &host, unsigned int port) {
    abac_host_ = host;
    abac_port_ = port;
  }

  void enable_abac(int conf) {
    abac_enabled_ = conf != 0;
  }

  void set_abac_id(const string &id) {
    abac_id_ = id;
  }
  void set_abac_principal_id(const string &id) {
    abac_principal_id_ = id;
  }
  void reset_abac(CURL** curl);
  bool check_abac_permission(const string &ip, unsigned int port);

  void set_abac_test(const string &ip, unsigned int port) {
    if (ip.size() != 0) {
      abac_test_ip_ = ip;
      abac_test_port_ = port;
    }
  }

  /** @brief Descriptive name of the connection routing */
  const string name;

  /** @brief Returns timeout when connecting to destination
   *
   * @return Timeout in seconds as int
   */
  int get_destination_connect_timeout() const noexcept {
    return destination_connect_timeout_;
  }

  /** @brief Sets timeout when connecting to destination
   *
   * Sets timeout connecting with destination servers. Timeout in seconds must be between 1 and
   * 65535.
   *
   * Throws std::invalid_argument when an invalid value was provided.
   *
   * @param seconds Timeout in seconds
   * @return New value as int
   */
  int set_destination_connect_timeout(int seconds);

  /** @brief Sets maximum active connections
   *
   * Sets maximum of active connections. Maximum must be between 1 and
   * 65535.
   *
   * Throws std::invalid_argument when an invalid value was provided.
   *
   * @param maximum Max number of connections allowed
   * @return New value as int
   */
  int set_max_connections(int maximum);

  /** @brief Checks and if needed, blocks a host from using this routing
   *
   * Blocks a host from using this routing adding its IP address to the
   * list of blocked hosts when the maximum client errors has been
   * reached. Each call of this function will increment the number of
   * times it was called with the client IP address.
   *
   * When a client host is actually blocked, true will be returned,
   * otherwise false.
   *
   * @param client_ip_array IP address as array[16] of uint8_t
   * @param client_ip_str IP address as string (for logging purposes)
   * @param server Server file descriptor to wish to send
   *               fake handshake reply (default is not to send anything)
   * @return bool
   */
  bool block_client_host(const std::array<uint8_t, 16> &client_ip_array,
                         const string &client_ip_str, int server = -1);

  /** @brief Returns a copy of the list of blocked client hosts
   *
   * Returns a copy of the list of the blocked client hosts.
   */
  const std::vector<std::array<uint8_t, 16>> get_blocked_client_hosts() {
    std::lock_guard<std::mutex> lock(mutex_auth_errors_);
    return std::vector<std::array<uint8_t, 16>>(blocked_client_hosts_);
  }

  /** @brief Returns maximum active connections
   *
   * @return Maximum as int
   */
  int get_max_connections() const noexcept {
    return max_connections_;
  }

  /** @brief Reads from sender and writes it back to receiver using select
   *
   * This function reads data from the sender socket and writes it back
   * to the receiver socket. It uses `select`.
   *
   * Checking the handshaking is done when the client first connects and
   * the server sends its handshake. The client replies and the server
   * should reply with an OK (or Error) packet. This packet should be
   * packet number 2. For secure connections, however, the client asks
   * to switch to SSL and we can't check further packages (we can't
   * decrypt). When SSL switch is detected, this function will set pktnr
   * to 2, so we assume the handshaking was OK.
   *
   * @param sender Descriptor of the sender
   * @param receiver Descriptor of the receiver
   * @param readfds Read descriptors used with FD_ISSET
   * @param buffer Buffer to use for storage
   * @param curr_pktnr Pointer to storage for sequence id of packet
   * @param handshake_done Whether handshake phase is finished or not
   * @param report_bytes_read Pointer to storage to report bytes read
   * @return 0 on success; -1 on error
   */
  static int copy_mysql_protocol_packets(int sender, int receiver, fd_set *readfds,
                                         mysql_protocol::Packet::vector_t &buffer, int *curr_pktnr,
                                         bool handshake_done, size_t *report_bytes_read,
                                         routing::SocketOperationsBase *socket_operations);

private:
  /** @brief Sets up the TCP service
   *
   * Sets up the TCP service binding to IP addresses and TCP port.
   *
   * Throws std::runtime_error on errors.
   *
   * @return
   */
  void setup_service();

  /** @brief Worker function for thread
   *
   * Worker function handling incoming connection from a MySQL client using
   * the select-method.
   *
   * Errors are logged.
   *
   * @param client socket descriptor fo the client connection
   * @param client_addr IP address as sin6_addr struct
   * @param timeout timeout in seconds
   */
  void routing_select_thread(int client, const in6_addr client_addr) noexcept;

  /** @brief Mode to use when getting next destination */
  routing::AccessMode mode_;
  /** @brief Maximum active connections
   *
   * Maximum number of incoming connections that will be accepted
   * by this MySQLRouter instances. There is no maximum for outgoing
   * connections since it is one-to-one with incoming.
   */
  int max_connections_;
  /** @brief Timeout connecting to destination
   *
   * This timeout is used when trying to connect with a destination
   * server. When the timeout is reached, another server will be
   * tried. It is good to leave this time out to 1 second or higher
   * if using an unstable network.
   */
  int destination_connect_timeout_;
  /** @brief Max connect errors blocking hosts when handshake not completed */
  unsigned long long max_connect_errors_;
  /** @brief Timeout waiting for handshake response from client */
  unsigned int client_connect_timeout_;
  /** @brief Size of buffer to store receiving packets */
  unsigned int net_buffer_length_;
  /** @brief IP address and TCP port for setting up TCP service */
  const TCPAddress bind_address_;
  /** @brief Socket descriptor of the service */
  int sock_server_;
  /** @brief Destination object to use when getting next connection */
  std::unique_ptr<RouteDestination> destination_;
  /** @brief Whether we were asked to stop */
  std::atomic<bool> stopping_;
  /** @brief Number of active routes */
  std::atomic<uint16_t> info_active_routes_;
  /** @brief Number of handled routes */
  std::atomic<uint64_t> info_handled_routes_;

  /** @brief ABAC based connection check */
  std::string abac_host_;
  unsigned int abac_port_;
  std::string abac_id_;
  std::string abac_principal_id_;
  bool abac_enabled_;
  std::string abac_test_ip_;
  unsigned int abac_test_port_;
 
  /** @brief Authentication error counters for IPv4 or IPv6 hosts */
  std::mutex mutex_auth_errors_;
  std::map<std::array<uint8_t, 16>, size_t> auth_error_counters_;
  std::vector<std::array<uint8_t, 16>> blocked_client_hosts_;

  /** @brief object handling the operations on network sockets */
  routing::SocketOperationsBase* socket_operations_;
  /** @brief ABAC connection handle using curl */
  CURL *abac_curl_handle_;
};

extern "C"
{
  extern mysql_harness::Plugin ROUTING_API harness_plugin_routing;
}

#endif // ROUTING_MYSQLROUTING_INCLUDED
