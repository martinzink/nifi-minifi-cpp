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

#include <asio/read.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/take.hpp>


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

  processFlowFile(handler, context, session, flow_file);
}

void FetchModbusTcp::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchModbusTcp::readDynamicPropertyKeys(const core::ProcessContext& context) {
  dynamic_property_keys_.clear();
  const std::vector<std::string> dynamic_prop_keys = context.getDynamicPropertyKeys();
  for (const auto& key : dynamic_prop_keys) {
    dynamic_property_keys_.emplace_back(core::PropertyDefinitionBuilder<>::createProperty(key).withDescription("auto generated").supportsExpressionLanguage(true).build());
  }
}

std::shared_ptr<core::FlowFile> FetchModbusTcp::getFlowFile(core::ProcessSession& session) const {
  if (hasIncomingConnections()) {
    return session.get();
  }
  return session.create();
}

std::unordered_map<std::string, std::unique_ptr<ReadModbusFunction>> FetchModbusTcp::getAddressMap(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  std::unordered_map<std::string, std::unique_ptr<ReadModbusFunction>> address_map{};
  for (const auto& dynamic_property : dynamic_property_keys_) {
    if (std::string dynamic_property_value{}; context.getDynamicProperty(dynamic_property, dynamic_property_value, flow_file)) {
      if (auto modbus_func = ReadModbusFunction::parse(++transaction_id_, dynamic_property_value); modbus_func)
        address_map.emplace(dynamic_property.getName(), std::move(modbus_func));
    }
  }
  return address_map;
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
    core::ProcessContext& context,
    core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) {
  std::unordered_map<std::string, std::string> result_map{};
  const auto address_map = getAddressMap(context, flow_file);
  if (address_map.empty()) {
    logger_->log_warn("There are no registers to query");
    return;
  }

  std::error_code operation_error{};
  // TODO(mzink) do shit
  if (operation_error) {
    connection_handler->reset();
    logger_->log_error("{}", operation_error.message());
    session.transfer(flow_file, Failure);
  } else {
    session.transfer(flow_file, Success);
  }
}

nonstd::expected<std::unordered_map<std::string, std::string>, std::error_code> FetchModbusTcp::readModbus(
    const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
    const std::unordered_map<std::string, std::unique_ptr<ReadModbusFunction>>& address_map) {
  nonstd::expected<std::unordered_map<std::string, std::string>, std::error_code> result;

  io_context_.restart();
  asio::co_spawn(io_context_,
    sendRequestsAndReadResponses(*connection_handler, address_map),
    [&result](const std::exception_ptr& exception_ptr, const auto res) {
      if (exception_ptr) {
        result = nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
      } else {
        result = res;
      }
    });
  io_context_.run();
  return result;
}

auto FetchModbusTcp::sendRequestsAndReadResponses(utils::net::ConnectionHandlerBase& connection_handler,
    const std::unordered_map<std::string, std::unique_ptr<ReadModbusFunction>>& address_map) -> asio::awaitable<nonstd::expected<std::unordered_map<std::string, std::string>, std::error_code>> {
  std::unordered_map<std::string, std::string> result;
  for (const auto& [variable, read_modbus_fn] : address_map) {
    auto response = co_await sendRequestAndReadResponse(connection_handler, *read_modbus_fn);
    if (!response) {
      co_return nonstd::make_unexpected(response.error());
    }
    result[variable] = *response;
  }
  co_return result;
}


auto FetchModbusTcp::sendRequestAndReadResponse(utils::net::ConnectionHandlerBase& connection_handler,
    const ReadModbusFunction& read_modbus_function) -> asio::awaitable<nonstd::expected<std::string, std::error_code>> {
  std::string result;
  if (auto connection_error = co_await connection_handler.setupUsableSocket(io_context_))  // NOLINT
    co_return nonstd::make_unexpected(connection_error);

  if (auto [write_error, bytes_written] = co_await connection_handler.write(asio::buffer(read_modbus_function.requestBytes())); write_error)
    co_return nonstd::make_unexpected(write_error);

  std::array<std::byte, 7> apu_buffer{};
  asio::mutable_buffer response_apu(apu_buffer.data(), 7);
  if (auto [read_error, bytes_read] = co_await connection_handler.read(response_apu); read_error)
    co_return nonstd::make_unexpected(read_error);

  const auto received_transaction_id = convertFromBigEndian<uint16_t>(std::span<const std::byte, 2>{apu_buffer | ranges::views::take(2)});
  const auto received_protocol = convertFromBigEndian<uint16_t>(std::span<const std::byte, 2>{apu_buffer | ranges::views::drop(2) | ranges::views::take(2)});
  const auto received_length = convertFromBigEndian<uint16_t>(std::span<const std::byte, 2>{apu_buffer | ranges::views::drop(4) | ranges::views::take(2)});
  const auto unit_id = static_cast<const uint8_t>(apu_buffer[6]);

  if (received_transaction_id != read_modbus_function.getTransactionId())
    co_return nonstd::make_unexpected(ModbusExceptionCode::InvalidTransactionId);
  if (received_protocol != 0)
    co_return nonstd::make_unexpected(ModbusExceptionCode::IllegalProtocol);
  if (unit_id != 0)
    co_return nonstd::make_unexpected(ModbusExceptionCode::InvalidSlaveId);
  if (received_length + 6 > 260 || received_length <= 1)
    co_return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);

  std::array<std::byte, 260-7> pdu_buffer{};
  asio::mutable_buffer response_pdu(pdu_buffer.data(), received_length-1);
  auto [read_error, bytes_read] = co_await connection_handler.read(response_pdu);
  if (read_error)
    co_return nonstd::make_unexpected(read_error);

  const auto pdu_span = std::span<std::byte>(pdu_buffer.data(), received_length-1);
  co_return read_modbus_function.serializeResponsePdu(pdu_span);
}

}  // namespace org::apache::nifi::minifi::modbus
