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

/**
 * BUG22020088
 *
 */

#include "cmd_exec.h"
#include "gtest_consoleoutput.h"
#include "logger.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "router_test_helpers.h"
#include "../../../router/src/router_app.h"
#include "../../../routing/src/mysql_routing.h"
#include "../../../routing/src/utils.h"

#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>
#ifndef _WIN32
# include <netinet/in.h>
#else
# include <WinSock2.h>
#endif

#include "gmock/gmock.h"

using std::string;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::StrEq;

string g_cwd;
Path g_origin;

// Used in tests; does not change for each test.
const string kDefaultRoutingConfig = "\ndestinations=127.0.0.1:3306\nmode=read-only\n";

class Bug22020088 : public ConsoleOutputTest {
protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("Bug22020088.ini");

  }

  void reset_config() {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << stage_dir->str() << "\n";
      ofs_config << "config_folder = " << stage_dir->str() << "\n\n";
      ofs_config.close();
    }
  }

  std::unique_ptr<Path> config_path;
};

TEST_F(Bug22020088, MissingBindAddressAndDefaultPort) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
      "in [routing]: either bind_port or bind_address is required"));
  }
}

TEST_F(Bug22020088, MissingPortInBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
     "in [routing]: no bind_port, and TCP port in bind_address is not valid"));
  }
}

TEST_F(Bug22020088, InvalidPortInBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1:999292\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
     "option bind_address in [routing] is incorrect (invalid TCP port: invalid characters or too long)"));
  }
}

TEST_F(Bug22020088, InvalidDefaultPort) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_port=23123124123123\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  try {
    r.start();
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(), StrEq(
     "option bind_port in [routing] needs value between 1 and 65535 inclusive, was '23123124123123'"));
  }
}

TEST_F(Bug22020088, BlockClientHost) {
  unsigned long long max_connect_errors = 2;
  unsigned int client_connect_timeout = 2;
  in6_addr client_addr1, client_addr2;
  client_addr1.s6_addr[15] = 1;
  client_addr2.s6_addr[15] = 2;
  auto client_ip_array1 = in6_addr_to_array(client_addr1);
  auto client_ip_array2 = in6_addr_to_array(client_addr2);

  MySQLRouting r(routing::AccessMode::kReadWrite, 7001, "127.0.0.1", "routing:connect_erros",
                 1, 1, max_connect_errors, client_connect_timeout);

  ASSERT_FALSE(r.block_client_host(client_ip_array1, string("::1")));
  ASSERT_THAT(ssout.str(), HasSubstr("1 authentication errors for ::1 (max 2)"));
  reset_ssout();
  ASSERT_TRUE(r.block_client_host(client_ip_array1, string("::1")));
  ASSERT_THAT(ssout.str(), HasSubstr("blocking client host ::1"));

  return;
  auto blocked_hosts = r.get_blocked_client_hosts();
  ASSERT_THAT(blocked_hosts[0], ContainerEq(client_ip_array1));

  ASSERT_FALSE(r.block_client_host(client_ip_array2, string("::2")));
  ASSERT_TRUE(r.block_client_host(client_ip_array2, string("::2")));

  blocked_hosts = r.get_blocked_client_hosts();
  ASSERT_THAT(blocked_hosts[0], ContainerEq(client_ip_array1));
  ASSERT_THAT(blocked_hosts[1], ContainerEq(client_ip_array2));
}

TEST_F(Bug22020088, BlockClientHostWithFakeResponse) {
  unsigned long long max_connect_errors = 2;
  unsigned int client_connect_timeout = 2;
  in6_addr client_addr1;
  client_addr1.s6_addr[15] = 1;
  auto client_ip_array1 = in6_addr_to_array(client_addr1);

  MySQLRouting r(routing::AccessMode::kReadWrite, 7001, "127.0.0.1", "routing:connect_erros",
                 1, 1, max_connect_errors, client_connect_timeout);

  std::FILE* fd_response = std::fopen("fake_response.data", "w");

  ASSERT_FALSE(r.block_client_host(client_ip_array1, string("::1"), fileno(fd_response)));
  std::fclose(fd_response);
#ifndef _WIN32
  // block_client_host() will not be able to write data to the file because in windows, the
  // syscall to writing to sockets is different than for files
  fd_response = std::fopen("fake_response.data", "r");

  auto fake_response = mysql_protocol::HandshakeResponsePacket(1, {}, "ROUTER", "", "fake_router_login");

  auto server_response = ssout.str();
  for (size_t i = 0; i < fake_response.size(); ++i) {
     ASSERT_EQ(fake_response.at(i), std::fgetc(fd_response));
  }
  std::fclose(fd_response);
#endif
  std::remove("fake_response.data");
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
