/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <new>
#include <random>
#include <string>

#include "SingleProcessorTestController.h"
#include "Catch.h"
#include "PutTCP.h"
#include "controllers/SSLContextService.h"
#include "core/ProcessSession.h"
#include "utils/net/TcpServer.h"
#include "utils/net/AsioCoro.h"
#include "utils/expected.h"
#include "utils/StringUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

using controllers::SSLContextService;

namespace {

class CancellableTcpServer : public utils::net::TcpServer {
 public:
  using utils::net::TcpServer::TcpServer;

  size_t getNumberOfSessions() const {
    return cancellable_timers_.size();
  }

  void cancelEverything() {
    for (auto& timer : cancellable_timers_)
      io_context_.post([=]{timer->cancel();});
  }

  asio::awaitable<void> listen() override {
    using asio::experimental::awaitable_operators::operator||;

    asio::ip::tcp::acceptor acceptor(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port_));
    if (port_ == 0)
      port_ = acceptor.local_endpoint().port();
    while (true) {
      auto [accept_error, socket] = co_await acceptor.async_accept(utils::net::use_nothrow_awaitable);
      if (accept_error) {
        logger_->log_error("Error during accepting new connection: %s", accept_error.message());
        break;
      }
      auto cancellable_timer = std::make_shared<asio::steady_timer>(io_context_);
      cancellable_timers_.push_back(cancellable_timer);
      if (ssl_data_)
        co_spawn(io_context_, secureSession(std::move(socket)) || wait_until_cancelled(cancellable_timer), asio::detached);
      else
        co_spawn(io_context_, insecureSession(std::move(socket)) || wait_until_cancelled(cancellable_timer), asio::detached);
    }
  }

 private:
  static asio::awaitable<void> wait_until_cancelled(std::shared_ptr<asio::steady_timer> timer) {
    timer->expires_at(asio::steady_timer::time_point::max());
    co_await utils::net::async_wait(*timer);
  }

  std::vector<std::shared_ptr<asio::steady_timer>> cancellable_timers_;
};

utils::net::SslData createSslDataForServer() {
  const std::filesystem::path executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  utils::net::SslData ssl_data;
  ssl_data.ca_loc = (executable_dir / "resources" / "ca_A.crt").string();
  ssl_data.cert_loc = (executable_dir / "resources" / "localhost_by_A.pem").string();
  ssl_data.key_loc = (executable_dir / "resources" / "localhost_by_A.pem").string();
  return ssl_data;
}
}  // namespace

class PutTCPTestFixture {
 public:
  PutTCPTestFixture() {
    LogTestController::getInstance().setTrace<PutTCP>();
    LogTestController::getInstance().setInfo<core::ProcessSession>();
    LogTestController::getInstance().setTrace<utils::net::Server>();
    put_tcp_->setProperty(PutTCP::Hostname, "${literal('localhost')}");
    put_tcp_->setProperty(PutTCP::Timeout, "200 ms");
    put_tcp_->setProperty(PutTCP::OutgoingMessageDelimiter, "\n");
  }

  ~PutTCPTestFixture() {
    stopServers();
  }

  void stopServers() {
    for (auto& [port, server] : listeners_) {
      auto& listener = server.listener_;
      auto& server_thread = server.server_thread_;
      if (listener)
        listener->stop();
      if (server_thread.joinable())
        server_thread.join();
      listener.reset();
    }
  }

  size_t getNumberOfActiveSessions(std::optional<uint16_t> port = std::nullopt) {
    if (auto session_aware_listener = dynamic_cast<CancellableTcpServer*>(getListener(port))) {
      return session_aware_listener->getNumberOfSessions();  // There is always one inactive session waiting for a new connection
    }
    return -1;
  }

  void closeActiveConnections() {
    for (auto& [port, server] : listeners_) {
      if (auto session_aware_listener = dynamic_cast<CancellableTcpServer*>(getListener(port))) {
        session_aware_listener->cancelEverything();
      }
    }
    std::this_thread::sleep_for(200ms);
  }

  auto trigger(std::string_view message, std::unordered_map<std::string, std::string> input_flow_file_attributes = {}) {
    return controller_.trigger(message, std::move(input_flow_file_attributes));
  }

  auto getContent(const auto& flow_file) {
    return controller_.plan->getContent(flow_file);
  }

  std::optional<utils::net::Message> tryDequeueReceivedMessage(std::optional<uint16_t> port = std::nullopt) {
    auto timeout = 200ms;
    auto interval = 10ms;

    auto start_time = std::chrono::system_clock::now();
    utils::net::Message result;
    while (start_time + timeout > std::chrono::system_clock::now()) {
      if (getListener(port)->tryDequeue(result))
        return result;
      std::this_thread::sleep_for(interval);
    }
    return std::nullopt;
  }

  void addSSLContextToPutTCP(const std::filesystem::path& ca_cert, const std::optional<std::filesystem::path>& client_cert_key) {
    const std::filesystem::path ca_dir = std::filesystem::path(minifi::utils::file::FileUtils::get_executable_dir()) / "resources";
    auto ssl_context_service_node = controller_.plan->addController("SSLContextService", "SSLContextService");
    REQUIRE(controller_.plan->setProperty(ssl_context_service_node, SSLContextService::CACertificate.getName(), (ca_dir / ca_cert).string()));
    if (client_cert_key) {
      REQUIRE(controller_.plan->setProperty(ssl_context_service_node, SSLContextService::ClientCertificate.getName(), (ca_dir / *client_cert_key).string()));
      REQUIRE(controller_.plan->setProperty(ssl_context_service_node, SSLContextService::PrivateKey.getName(), (ca_dir / *client_cert_key).string()));
    }
    ssl_context_service_node->enable();

    put_tcp_->setProperty(PutTCP::SSLContextService, "SSLContextService");
  }

  void setHostname(const std::string& hostname) {
    REQUIRE(controller_.plan->setProperty(put_tcp_, PutTCP::Hostname.getName(), hostname));
  }

  void enableConnectionPerFlowFile() {
    REQUIRE(controller_.plan->setProperty(put_tcp_, PutTCP::ConnectionPerFlowFile.getName(), "true"));
  }

  void setIdleConnectionExpiration(const std::string& idle_connection_expiration_str) {
    REQUIRE(controller_.plan->setProperty(put_tcp_, PutTCP::IdleConnectionExpiration.getName(), idle_connection_expiration_str));
  }

  uint16_t addTCPServer() {
    Server server;
    uint16_t port = server.startTCPServer(std::nullopt);
    listeners_[port] = std::move(server);
    return port;
  }

  uint16_t addSSLServer() {
    auto ssl_server_options = utils::net::SslServerOptions{createSslDataForServer(), utils::net::ClientAuthOption::REQUIRED};
    Server server;
    uint16_t port = server.startTCPServer(ssl_server_options);
    listeners_[port] = std::move(server);
    return port;
  }

  void setPutTCPPort(uint16_t port) {
    put_tcp_->setProperty(PutTCP::Port, utils::StringUtils::join_pack("${literal('", std::to_string(port), "')}"));
  }

  void setPutTCPPort(std::string port_str) {
    put_tcp_->setProperty(PutTCP::Port, std::move(port_str));
  }

  [[nodiscard]] uint16_t getSinglePort() const {
    gsl_Expects(listeners_.size() == 1);
    return listeners_.begin()->first;
  }

 private:
  utils::net::Server* getListener(std::optional<uint16_t> port) {
    if (!port)
      port = getSinglePort();
    return listeners_.at(*port).listener_.get();
  }

  const std::shared_ptr<PutTCP> put_tcp_ = std::make_shared<PutTCP>("PutTCP");
  test::SingleProcessorTestController controller_{put_tcp_};

  class Server {
   public:
    Server() = default;

    uint16_t startTCPServer(std::optional<utils::net::SslServerOptions> ssl_server_options) {
      gsl_Expects(!listener_ && !server_thread_.joinable());
      listener_ = std::make_unique<CancellableTcpServer>(std::nullopt, 0, core::logging::LoggerFactory<utils::net::Server>::getLogger(), std::move(ssl_server_options));
      server_thread_ = std::thread([this]() { listener_->run(); });
      uint16_t port = listener_->getPort();
      auto deadline = std::chrono::steady_clock::now() + 200ms;
      while (port == 0 && deadline > std::chrono::steady_clock::now()) {
        std::this_thread::sleep_for(20ms);
        port = listener_->getPort();
      }
      REQUIRE(port != 0);
      return port;
    }

    std::unique_ptr<utils::net::Server> listener_;
    std::thread server_thread_;
  };
  std::unordered_map<uint16_t, Server> listeners_;
};

void trigger_expect_success(PutTCPTestFixture& test_fixture, const std::string_view message, std::unordered_map<std::string, std::string> input_flow_file_attributes = {}) {
  const auto result = test_fixture.trigger(message, std::move(input_flow_file_attributes));
  const auto& success_flow_files = result.at(PutTCP::Success);
  CHECK(success_flow_files.size() == 1);
  CHECK(result.at(PutTCP::Failure).empty());
  if (!success_flow_files.empty())
    CHECK(test_fixture.getContent(success_flow_files[0]) == message);
}

void trigger_expect_failure(PutTCPTestFixture& test_fixture, const std::string_view message) {
  const auto result = test_fixture.trigger(message);
  const auto &failure_flow_files = result.at(PutTCP::Failure);
  CHECK(failure_flow_files.size() == 1);
  CHECK(result.at(PutTCP::Success).empty());
  if (!failure_flow_files.empty())
    CHECK(test_fixture.getContent(failure_flow_files[0]) == message);
}

void receive_success(PutTCPTestFixture& test_fixture, const std::string_view expected_message, std::optional<uint16_t> port = std::nullopt) {
  auto received_message = test_fixture.tryDequeueReceivedMessage(port);
  CHECK(received_message);
  if (received_message) {
    CHECK(received_message->message_data == expected_message);
    CHECK(received_message->protocol == utils::net::IpProtocol::TCP);
    CHECK(!received_message->sender_address.to_string().empty());
  }
}

constexpr std::string_view first_message = "message 1";
constexpr std::string_view second_message = "message 22";
constexpr std::string_view third_message = "message 333";
constexpr std::string_view fourth_message = "message 4444";
constexpr std::string_view fifth_message = "message 55555";
constexpr std::string_view sixth_message = "message 666666";

TEST_CASE("Server closes in-use socket", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    auto port = test_fixture.addTCPServer();
    test_fixture.setPutTCPPort(port);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    auto port = test_fixture.addSSLServer();
    test_fixture.setPutTCPPort(port);
  }

  trigger_expect_success(test_fixture, first_message);
  trigger_expect_success(test_fixture, second_message);
  trigger_expect_success(test_fixture, third_message);

  receive_success(test_fixture, first_message);
  receive_success(test_fixture, second_message);
  receive_success(test_fixture, third_message);

  CHECK(1 == test_fixture.getNumberOfActiveSessions());

  test_fixture.closeActiveConnections();

  trigger_expect_success(test_fixture, fourth_message);
  trigger_expect_success(test_fixture, fifth_message);
  trigger_expect_success(test_fixture, sixth_message);

  test_fixture.tryDequeueReceivedMessage();

  CHECK(LogTestController::getInstance().matchesRegex("warning.*with reused connection, retrying"));
  CHECK(2 == test_fixture.getNumberOfActiveSessions());
}

TEST_CASE("Connection per flow file", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    auto port = test_fixture.addTCPServer();
    test_fixture.setPutTCPPort(port);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    auto port = test_fixture.addSSLServer();
    test_fixture.setPutTCPPort(port);
  }

  test_fixture.enableConnectionPerFlowFile();

  trigger_expect_success(test_fixture, first_message);
  trigger_expect_success(test_fixture, second_message);
  trigger_expect_success(test_fixture, third_message);

  receive_success(test_fixture, first_message);
  receive_success(test_fixture, second_message);
  receive_success(test_fixture, third_message);

  trigger_expect_success(test_fixture, fourth_message);
  trigger_expect_success(test_fixture, fifth_message);
  trigger_expect_success(test_fixture, sixth_message);

  receive_success(test_fixture, fourth_message);
  receive_success(test_fixture, fifth_message);
  receive_success(test_fixture, sixth_message);

  CHECK(6 == test_fixture.getNumberOfActiveSessions());
}

TEST_CASE("PutTCP test invalid host", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
  }

  test_fixture.setPutTCPPort(1235);
  test_fixture.setHostname("invalid_hostname");
  trigger_expect_failure(test_fixture, "message for invalid host");

  CHECK((LogTestController::getInstance().contains("Host not found", 0ms)
      || LogTestController::getInstance().contains("No such host is known", 0ms)
      || LogTestController::getInstance().contains("A connection attempt failed because the connected party did not properly respond", 0ms)));
}

TEST_CASE("PutTCP test invalid server", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
  }
  test_fixture.setPutTCPPort(1235);
  test_fixture.setHostname("localhost");
  trigger_expect_failure(test_fixture, "message for invalid server");

  CHECK((LogTestController::getInstance().contains("Connection refused", 0ms)
      || LogTestController::getInstance().contains("No connection could be made because the target machine actively refused it", 0ms)
      || LogTestController::getInstance().contains("A connection attempt failed because the connected party did not properly respond", 0ms)));
}

TEST_CASE("PutTCP test non-routable server", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
  }
  test_fixture.setHostname("192.168.255.255");
  test_fixture.setPutTCPPort(1235);
  trigger_expect_failure(test_fixture, "message for non-routable server");

  CHECK((LogTestController::getInstance().contains("No route to host", 0ms)
      || LogTestController::getInstance().contains("Connection timed out", 0ms)
      || LogTestController::getInstance().contains("Operation timed out", 0ms)
      || LogTestController::getInstance().contains("host has failed to respond", 0ms)));
}

TEST_CASE("PutTCP test invalid server cert", "[PutTCP]") {
  PutTCPTestFixture test_fixture;

  test_fixture.addSSLContextToPutTCP("ca_B.crt", "alice_by_B.pem");
  test_fixture.setHostname("localhost");
  auto port = test_fixture.addSSLServer();
  test_fixture.setPutTCPPort(port);

  trigger_expect_failure(test_fixture, "message for invalid-cert server");

  CHECK(LogTestController::getInstance().matchesRegex("Handshake with .* failed", 0ms));
}

TEST_CASE("PutTCP test missing client cert", "[PutTCP]") {
  PutTCPTestFixture test_fixture;

  test_fixture.addSSLContextToPutTCP("ca_A.crt", std::nullopt);
  test_fixture.setHostname("localhost");
  auto port = test_fixture.addSSLServer();
  test_fixture.setPutTCPPort(port);

  trigger_expect_failure(test_fixture, "message for invalid-cert server");

  CHECK(LogTestController::getInstance().matchesRegex("Handshake with .* failed", 0ms));
}

TEST_CASE("PutTCP test idle connection expiration", "[PutTCP]") {
  PutTCPTestFixture test_fixture;

  SECTION("No SSL") {
    auto port = test_fixture.addTCPServer();
    test_fixture.setPutTCPPort(port);
  }
  SECTION("SSL") {
    auto port = test_fixture.addSSLServer();
    test_fixture.setPutTCPPort(port);
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
  }

  test_fixture.setIdleConnectionExpiration("100ms");
  trigger_expect_success(test_fixture, first_message);
  std::this_thread::sleep_for(110ms);
  trigger_expect_success(test_fixture, second_message);

  receive_success(test_fixture, first_message);
  receive_success(test_fixture, second_message);

  CHECK(2 == test_fixture.getNumberOfActiveSessions());
}

TEST_CASE("PutTCP test long flow file chunked sending", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    auto port = test_fixture.addTCPServer();
    test_fixture.setPutTCPPort(port);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    auto port = test_fixture.addSSLServer();
    test_fixture.setPutTCPPort(port);
  }
  std::string long_message(3500, 'a');
  trigger_expect_success(test_fixture, long_message);
  receive_success(test_fixture, long_message);
}

TEST_CASE("PutTCP test multiple servers", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  size_t number_of_servers = 5;
  std::vector<uint16_t> ports;
  SECTION("No SSL") {
    for (size_t i = 0; i < number_of_servers; ++i) {
      ports.push_back(test_fixture.addTCPServer());
    }
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    for (size_t i = 0; i < number_of_servers; ++i) {
      ports.push_back(test_fixture.addSSLServer());
    }
  }

  test_fixture.setPutTCPPort("${tcp_port}");

  for (auto i = 0; i < 3; ++i) {
    for (auto& port : ports) {
      std::string message = "Test message ";
      message.append(std::to_string(port));
      trigger_expect_success(test_fixture, message, {{"tcp_port", std::to_string(port)}});
      receive_success(test_fixture, message, port);
    }
  }
  for (auto& port : ports) {
    CHECK(1 == test_fixture.getNumberOfActiveSessions(port));
  }
}
}  // namespace org::apache::nifi::minifi::processors
