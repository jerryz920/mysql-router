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

#ifndef PLUGIN_CONFIG_ROUTING_INCLUDED
#define PLUGIN_CONFIG_ROUTING_INCLUDED

#include "utils.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing.h"

#include "mysqlrouter/plugin_config.h"
#include "mysql/harness/plugin.h"
#include "utils.h"

#include <map>
#include <string>

using std::map;
using std::string;
using mysqlrouter::to_string;
using mysqlrouter::TCPAddress;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;

class RoutingPluginConfig final : public mysqlrouter::BasePluginConfig {
public:
  /** @brief Constructor
   *
   * @param section from configuration file provided as ConfigSection
   */
  RoutingPluginConfig(const mysql_harness::ConfigSection *section)
      : BasePluginConfig(section),
        destinations(get_option_destinations(section, "destinations")),
        bind_port(get_option_tcp_port(section, "bind_port")),
        bind_address(get_option_tcp_address(section, "bind_address", false, bind_port)),
        connect_timeout(get_uint_option<uint16_t>(section, "connect_timeout", 1)),
        mode(get_option_mode(section, "mode")),
        max_connections(get_uint_option<uint16_t>(section, "max_connections", 1)),
        max_connect_errors(get_uint_option<uint32_t>(section, "max_connect_errors", 1, UINT32_MAX)),
        client_connect_timeout(get_uint_option<uint32_t>(section, "client_connect_timeout", 2, 31536000)),
        net_buffer_length(get_uint_option<uint32_t>(section, "net_buffer_length", 1024, 1048576)),
      abac_host(get_option_string(section, "abac_host")),
      abac_port(get_uint_option<uint32_t>(section, "abac_port", 0, 65535)),
      abac_id(get_option_string(section, "abac_id")),
      abac_principal_id(get_option_string(section, "abac_principal_id")),
      abac_enabled(get_uint_option<uint32_t>(section, "abac_enabled", 0, 2)),
      abac_test_ip(get_option_string(section, "abac_test_ip")),
      abac_test_port(get_uint_option<uint32_t>(section, "abac_test_port", 1, 65535)) { }

  string get_default(const string &option);

  bool is_required(const string &option);

  /** @brief `destinations` option read from configuration section */
  const string destinations;
  /** @brief `bind_port` option read from configuration section */
  const int bind_port;
  /** @brief `bind_address` option read from configuration section */
  const TCPAddress bind_address;
  /** @brief `connect_timeout` option read from configuration section */
  const int connect_timeout;
  /** @brief `mode` option read from configuration section */
  const routing::AccessMode mode;
  /** @brief `max_connections` option read from configuration section */
  const int max_connections;
  /** @brief `max_connect_errors` option read from configuration section */
  const unsigned long long max_connect_errors;
  /** @brief `client_connect_timeout` option read from configuration section */
  const unsigned int client_connect_timeout;
  /** @brief Size of buffer to receive packets */
  const unsigned int net_buffer_length;
  /** @brief attestation based access control host, deciding whether allow connection */
  const std::string abac_host;
  /** @brief attestation based access control port, deciding whether allow connection */
  const unsigned int abac_port;
  /** @brief attestation based access control, sources */
  const std::string abac_src_whitelist;
  /** @brief abac ID set for this router */
  const std::string abac_id;
  /** @brief abac principal ID set for this router */
  const std::string abac_principal_id;
  /** @brief whether to enable the ABAC */
  const bool abac_enabled;

  /** @brief used for just test purpose */
  const std::string abac_test_ip;
  const unsigned int abac_test_port;

protected:
  
private:
  routing::AccessMode get_option_mode(const mysql_harness::ConfigSection *section, const string &option);
  string get_option_destinations(const mysql_harness::ConfigSection *section, const string &option);
};

#endif // PLUGIN_CONFIG_ROUTING_INCLUDED
