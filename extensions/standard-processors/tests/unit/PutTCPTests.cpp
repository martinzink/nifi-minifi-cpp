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
#include "utils/net/SslServer.h"
#include "utils/expected.h"
#include "utils/StringUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::processors {

using controllers::SSLContextService;

namespace {
using utils::net::TcpSession;
using utils::net::TcpServer;

using utils::net::SslSession;
using utils::net::SslServer;

class ISessionAwareServer {
 public:
  [[nodiscard]] virtual size_t getNumberOfSessions() const = 0;
  virtual void closeSessions() = 0;
};

template<class SocketType>
class SessionAwareServer : public ISessionAwareServer {
 protected:
  size_t getNumberOfSessions() const override {
    std::lock_guard lock_guard{mutex_};
    return sessions_.size();
  }

  void closeSessions() override {
    std::lock_guard lock_guard{mutex_};
    for (const auto& session_weak : sessions_) {
      if (auto session = session_weak.lock()) {
        auto& socket = session->getSocket();
        if (socket.is_open()) {
          socket.shutdown(asio::ip::tcp::socket::shutdown_both);
          session->getSocket().close();
        }
      }
    }
  }

  mutable std::mutex mutex_;
  std::vector<std::weak_ptr<SocketType>> sessions_;
};

class SessionAwareTcpServer : public TcpServer, public SessionAwareServer<TcpSession> {
 public:
  using TcpServer::TcpServer;

 protected:
  std::shared_ptr<TcpSession> createSession() override {
    std::lock_guard lock_guard{mutex_};
    auto session = TcpServer::createSession();
    logger_->log_trace("SessionAwareTcpServer::createSession %p", session.get());
    sessions_.emplace_back(session);
    return session;
  }
};

class SessionAwareSslServer : public SslServer, public SessionAwareServer<SslSession> {
 public:
  using SslServer::SslServer;

 protected:
  std::shared_ptr<SslSession> createSession() override {
    std::lock_guard lock_guard{mutex_};
    auto session = SslServer::createSession();
    logger_->log_trace("SessionAwareTcpServer::createSession %p", session.get());
    sessions_.emplace_back(session);
    return session;
  }
};

utils::net::SslData createSslDataForServer() {
  const std::filesystem::path executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  utils::net::SslData ssl_data;
  ssl_data.ca_loc = (executable_dir / "resources/ca_A.crt").string();
  ssl_data.cert_loc = (executable_dir / "resources/localhost_by_A.pem").string();
  ssl_data.key_loc = (executable_dir / "resources/localhost_by_A.pem").string();
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
    put_tcp_->setProperty(PutTCP::Port, utils::StringUtils::join_pack("${literal('", std::to_string(port_), "')}"));
    put_tcp_->setProperty(PutTCP::Timeout, "200 ms");
    put_tcp_->setProperty(PutTCP::OutgoingMessageDelimiter, "\n");
  }

  ~PutTCPTestFixture() {
    stopServer();
  }

  void startTCPServer() {
    gsl_Expects(!listener_ && !server_thread_.joinable());
    listener_ = std::make_unique<SessionAwareTcpServer>(std::nullopt, port_, core::logging::LoggerFactory<utils::net::Server>().getLogger());
    server_thread_ = std::thread([this]() { listener_->run(); });
  }

  void startSSLServer() {
    gsl_Expects(!listener_ && !server_thread_.joinable());
    listener_ = std::make_unique<SessionAwareSslServer>(std::nullopt,
        port_,
        core::logging::LoggerFactory<utils::net::Server>().getLogger(),
        createSslDataForServer(),
        utils::net::SslServer::ClientAuthOption::REQUIRED);
    server_thread_ = std::thread([this]() { listener_->run(); });
  }

  void stopServer() {
    if (listener_)
      listener_->stop();
    if (server_thread_.joinable())
      server_thread_.join();
    listener_.reset();
  }

  size_t getNumberOfActiveSessions() {
    if (auto session_aware_listener = dynamic_cast<ISessionAwareServer*>(listener_.get())) {
      return session_aware_listener->getNumberOfSessions() - 1;  // There is always one inactive session waiting for a new connection
    }
    return -1;
  }

  void closeActiveConnections() {
    if (auto session_aware_listener = dynamic_cast<ISessionAwareServer*>(listener_.get())) {
      session_aware_listener->closeSessions();
    }
    std::this_thread::sleep_for(200ms);
  }

  auto trigger(const std::string_view& message) {
    return controller_.trigger(message);
  }

  auto getContent(const auto& flow_file) {
    return controller_.plan->getContent(flow_file);
  }

  std::optional<utils::net::Message> tryDequeueReceivedMessage() {
    auto timeout = 200ms;
    auto interval = 10ms;

    auto start_time = std::chrono::system_clock::now();
    utils::net::Message result;
    while (start_time + timeout > std::chrono::system_clock::now()) {
      if (listener_->tryDequeue(result))
        return result;
      std::this_thread::sleep_for(interval);
    }
    return std::nullopt;
  }

  void addSSLContextToPutTCP(const std::filesystem::path& ca_cert, std::optional<std::filesystem::path> client_cert_key) {
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

 private:
  const std::shared_ptr<PutTCP> put_tcp_ = std::make_shared<PutTCP>("PutTCP");
  test::SingleProcessorTestController controller_{put_tcp_};

  std::mt19937 random_engine_{std::random_device{}()};  // NOLINT: "Missing space before {  [whitespace/braces] [5]"
  // most systems use ports 32768 - 65535 as ephemeral ports, so avoid binding to those
  const uint16_t port_ = std::uniform_int_distribution<uint16_t>{10000, 32768 - 1}(random_engine_);

  std::unique_ptr<utils::net::Server> listener_;
  std::thread server_thread_;
};

void trigger_expect_success(PutTCPTestFixture& test_fixture, const std::string_view message) {
  const auto result = test_fixture.trigger(message);
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

void receive_success(PutTCPTestFixture& test_fixture, const std::string_view expected_message)  {
  auto received_message = test_fixture.tryDequeueReceivedMessage();
  CHECK(received_message);
  if (received_message) {
    CHECK(received_message->message_data == expected_message);
    CHECK(received_message->protocol == utils::net::IpProtocol::TCP);
    CHECK(!received_message->sender_address.to_string().empty());
  }
}

void test_invalid_host(PutTCPTestFixture& test_fixture) {
  test_fixture.setHostname("invalid_hostname");
  trigger_expect_failure(test_fixture, "message for invalid host");

  CHECK((LogTestController::getInstance().contains("Host not found")
      || LogTestController::getInstance().contains("No such host is known")));
}

void test_invalid_server(PutTCPTestFixture& test_fixture) {
  test_fixture.setHostname("localhost");
  trigger_expect_failure(test_fixture, "message for invalid server");

  CHECK((LogTestController::getInstance().contains("Connection refused")
      || LogTestController::getInstance().contains("No connection could be made because the target machine actively refused it")));
}

void test_non_routable_server(PutTCPTestFixture& test_fixture) {
  test_fixture.setHostname("192.168.255.255");
  trigger_expect_failure(test_fixture, "message for non-routable server");

  CHECK((LogTestController::getInstance().contains("Connection timed out")
      || LogTestController::getInstance().contains("Operation timed out")
      || LogTestController::getInstance().contains("host has failed to respond")));
}

void test_cert_verify_failure(PutTCPTestFixture& test_fixture) {
  test_fixture.setHostname("localhost");
  test_fixture.startSSLServer();

  trigger_expect_failure(test_fixture, "message for invalid-cert server");

  CHECK((LogTestController::getInstance().contains("certificate verify failed")
      || LogTestController::getInstance().contains("asio.ssl error")));
}

void test_handshake_failure(PutTCPTestFixture& test_fixture) {
  test_fixture.setHostname("localhost");
  test_fixture.startSSLServer();

  trigger_expect_failure(test_fixture, "message for invalid-cert server");

  CHECK((LogTestController::getInstance().contains("sslv3 alert handshake failure")
      || LogTestController::getInstance().contains("asio.ssl error")));
}

constexpr std::string_view first_message = "message 1";
constexpr std::string_view second_message = "message 22";
constexpr std::string_view third_message = "message 333";
constexpr std::string_view fourth_message = "message 4444";
constexpr std::string_view fifth_message = "message 55555";
constexpr std::string_view sixth_message = "message 666666";

void test_dropped_in_use_socket(PutTCPTestFixture& test_fixture) {
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

void test_connection_per_flow_file(PutTCPTestFixture& test_fixture) {
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

void test_idle_expiration(PutTCPTestFixture& test_fixture) {
  test_fixture.setIdleConnectionExpiration("100ms");
  trigger_expect_success(test_fixture, first_message);
  std::this_thread::sleep_for(110ms);
  trigger_expect_success(test_fixture, second_message);

  receive_success(test_fixture, first_message);
  receive_success(test_fixture, second_message);

  CHECK(2 == test_fixture.getNumberOfActiveSessions());
}

TEST_CASE("Server closes in-use socket", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    test_fixture.startTCPServer();
    test_dropped_in_use_socket(test_fixture);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    test_fixture.startSSLServer();

    test_dropped_in_use_socket(test_fixture);
  }
}

TEST_CASE("Connection per flow file", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    test_fixture.startTCPServer();
    test_connection_per_flow_file(test_fixture);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    test_fixture.startSSLServer();

    test_connection_per_flow_file(test_fixture);
  }
}

TEST_CASE("PutTCP test invalid host", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    test_invalid_host(test_fixture);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    test_invalid_host(test_fixture);
  }
}

TEST_CASE("PutTCP test invalid server", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    test_invalid_server(test_fixture);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    test_invalid_server(test_fixture);
  }
}

TEST_CASE("PutTCP test non-routable server", "[PutTCP]") {
  PutTCPTestFixture test_fixture;
  SECTION("No SSL") {
    test_non_routable_server(test_fixture);
  }
  SECTION("SSL") {
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    test_non_routable_server(test_fixture);
  }
}

TEST_CASE("PutTCP test invalid server cert", "[PutTCP]") {
  PutTCPTestFixture test_fixture;

  test_fixture.addSSLContextToPutTCP("ca_B.crt", "alice_by_B.pem");
  test_cert_verify_failure(test_fixture);
}

TEST_CASE("PutTCP test missing client cert", "[PutTCP]") {
  PutTCPTestFixture test_fixture;

  test_fixture.addSSLContextToPutTCP("ca_A.crt", std::nullopt);
  test_handshake_failure(test_fixture);
}

TEST_CASE("PutTCP test idle connection expiration", "[PutTCP]") {
  PutTCPTestFixture test_fixture;

  SECTION("No SSL") {
    test_fixture.startTCPServer();
    test_idle_expiration(test_fixture);
  }
  SECTION("SSL") {
    test_fixture.startSSLServer();
    test_fixture.addSSLContextToPutTCP("ca_A.crt", "alice_by_A.pem");
    test_idle_expiration(test_fixture);
  }
}

}  // namespace org::apache::nifi::minifi::processors
