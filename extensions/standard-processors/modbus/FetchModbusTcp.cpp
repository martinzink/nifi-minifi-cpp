/**
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

#include "FetchModbusTcp.h"

#include <utils/net/ConnectionHandler.h>

#include "core/ProcessSession.h"
#include "modbus/Error.h"
#include "modbus/ReadModbusFunctions.h"
#include "utils/net/AsioCoro.h"
#include "utils/net/AsioSocketUtils.h"

using asio::ip::tcp;

using namespace std::literals::chrono_literals;
using std::chrono::steady_clock;

namespace org::apache::nifi::minifi::modbus {


void FetchModbusTcp::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
    // if the required properties are missing or empty even before evaluating the EL expression, then we can throw in onSchedule, before we waste any flow files
  if (context.getProperty(Hostname).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing hostname"};
  }
  if (context.getProperty(Port).value_or(std::string{}).empty()) {
    throw Exception{ExceptionType::PROCESSOR_EXCEPTION, "missing port"};
  }
  if (const auto idle_connection_expiration = context.getProperty<core::TimePeriodValue>(IdleConnectionExpiration); idle_connection_expiration && idle_connection_expiration->getMilliseconds() > 0ms)
    idle_connection_expiration_ = idle_connection_expiration->getMilliseconds();
  else
    idle_connection_expiration_.reset();

  if (const auto timeout = context.getProperty<core::TimePeriodValue>(Timeout); timeout && timeout->getMilliseconds() > 0ms)
    timeout_duration_ = timeout->getMilliseconds();
  else
    timeout_duration_ = 15s;

  if (context.getProperty<bool>(ConnectionPerFlowFile).value_or(false))
    connections_.reset();
  else
    connections_.emplace();

  ssl_context_.reset();
  if (const auto context_name = context.getProperty(SSLContextService); context_name && !IsNullOrEmpty(*context_name)) {
    if (auto controller_service = context.getControllerService(*context_name)) {
      if (const auto ssl_context_service = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*context_name))) {
        ssl_context_ = utils::net::getSslContext(*ssl_context_service);
      } else {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, *context_name + " is not an SSL Context Service");
      }
    } else {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid controller service: " + *context_name);
    }
  }
}

void FetchModbusTcp::onTrigger(core::ProcessContext&  context, core::ProcessSession& session) {
  const auto flow_file = getFlowFile(session);
  if (!flow_file) {
    yield();
    return;
  }

  removeExpiredConnections();

  auto hostname = context.getProperty(Hostname, flow_file).value_or(std::string{});
  auto port = context.getProperty(Port, flow_file).value_or(std::string{});
  if (hostname.empty() || port.empty()) {
    logger_->log_error("[{}] invalid target endpoint: hostname: {}, port: {}", flow_file->getUUIDStr(),
        hostname.empty() ? "(empty)" : hostname.c_str(),
        port.empty() ? "(empty)" : port.c_str());
    session.transfer(flow_file, Failure);
    return;
  }

  auto connection_id = utils::net::ConnectionId(std::move(hostname), std::move(port));
  std::shared_ptr<utils::net::ConnectionHandlerBase> handler;
  if (!connections_ || !connections_->contains(connection_id)) {
    if (ssl_context_)
      handler = std::make_shared<utils::net::ConnectionHandler<utils::net::SslSocket>>(connection_id, timeout_duration_, logger_, max_size_of_socket_send_buffer_, &*ssl_context_);
    else
      handler = std::make_shared<utils::net::ConnectionHandler<utils::net::TcpSocket>>(connection_id, timeout_duration_, logger_, max_size_of_socket_send_buffer_, nullptr);
    if (connections_)
      (*connections_)[connection_id] = handler;
  } else {
    handler = (*connections_)[connection_id];
  }

  gsl_Expects(handler);

  processFlowFile(handler, session, flow_file);
}

void FetchModbusTcp::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

std::shared_ptr<core::FlowFile> FetchModbusTcp::getFlowFile(core::ProcessSession& session) const {
  if (hasIncomingConnections()) {
    return session.get();
  }
  return session.create();
}

std::unordered_map<std::string, std::string> FetchModbusTcp::getAddressMap(const core::FlowFile& flow_file) const {
  return {};
}

void FetchModbusTcp::removeExpiredConnections() {
  if (connections_) {
    std::erase_if(*connections_, [this](auto& item) -> bool {
      const auto& connection_handler = item.second;
      return (!connection_handler || (idle_connection_expiration_ && !connection_handler->hasBeenUsedIn(*idle_connection_expiration_)));
    });
  }
}

void FetchModbusTcp::processFlowFile(const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
    core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) const {
  const auto address_map = getAddressMap(*flow_file);

  std::error_code operation_error{};


  if (operation_error) {
    connection_handler->reset();
    logger_->log_error("{}", operation_error.message());
    session.transfer(flow_file, Failure);
  } else {
    session.transfer(flow_file, Success);
  }
}

}  // namespace org::apache::nifi::minifi::modbus
