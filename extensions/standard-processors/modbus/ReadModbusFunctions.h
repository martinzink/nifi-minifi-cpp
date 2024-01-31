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

#pragma once


#include <vector>

#include "modbus/ByteConverters.h"
#include "modbus/Error.h"
#include "range/v3/algorithm/copy.hpp"
#include "range/v3/view/chunk.hpp"
#include "range/v3/view/subrange.hpp"
#include "utils/expected.h"
#include "fmt/format.h"

namespace org::apache::nifi::minifi::modbus {

class ReadModbusFunction {
 public:
  explicit ReadModbusFunction(const uint16_t transaction_id) : transaction_id_(transaction_id) {
  }
  ReadModbusFunction(const ReadModbusFunction&) = delete;
  ReadModbusFunction(ReadModbusFunction&&) = delete;
  ReadModbusFunction& operator=(ReadModbusFunction&&) = delete;
  ReadModbusFunction& operator=(const ReadModbusFunction&) = delete;

  virtual ~ReadModbusFunction() = default;

  [[nodiscard]] std::vector<std::byte> requestBytes() const {
    constexpr std::array modbus_service_protocol_identifier = {std::byte{0}, std::byte{0}};
    constexpr std::byte unit_identifier{0};
    const auto pdu = rawPdu();
    const uint16_t length = pdu.size() + 1;

    std::vector<std::byte> request;
    ranges::copy(convertToBigEndian(transaction_id_), std::back_inserter(request));
    ranges::copy(modbus_service_protocol_identifier, std::back_inserter(request));
    ranges::copy(convertToBigEndian(length), std::back_inserter(request));
    request.push_back(unit_identifier);
    ranges::copy(pdu, std::back_inserter(request));
    return request;
  }

  [[nodiscard]] uint16_t getTransactionId() const { return transaction_id_; }
  [[nodiscard]] virtual nonstd::expected<std::string, std::error_code> serializeResponsePdu(std::span<const std::byte> resp_pdu) const = 0;

  static std::unique_ptr<ReadModbusFunction> parse(const uint16_t, std::string_view) { return nullptr; }

 protected:
  [[nodiscard]] auto getRespBytes(std::span<const std::byte> resp_pdu) const -> nonstd::expected<ranges::subrange<std::vector<std::byte>::const_iterator>, std::error_code> {
    if (resp_pdu.size() < 2) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    if (const auto resp_function_code = resp_pdu.front(); resp_function_code != getFunctionCode()) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    const uint8_t resp_byte_count = static_cast<uint8_t>(resp_pdu[1]);
    constexpr size_t function_code_length = 1;
    constexpr size_t unit_id_length = 1;
    if (resp_pdu.size() != resp_byte_count + function_code_length + unit_id_length) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    if (resp_byte_count != expectedLength()) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    return ranges::subrange<std::vector<std::byte>::const_iterator>(resp_pdu.begin() + 2, resp_pdu.end());
  }

  [[nodiscard]] virtual std::byte getFunctionCode() const = 0;
  [[nodiscard]] virtual std::vector<std::byte> rawPdu() const = 0;
  [[nodiscard]] virtual size_t expectedLength() const = 0;

  const uint16_t transaction_id_;
};

class ReadCoilStatus final : public ReadModbusFunction {
 public:
  ReadCoilStatus(const uint16_t transaction_id, const uint16_t starting_address, const uint16_t number_of_points)
      : ReadModbusFunction(transaction_id),
        starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(getFunctionCode());
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  [[nodiscard]] nonstd::expected<std::vector<bool>, std::error_code> handleResponse(const std::span<const std::byte> resp_pdu) const {
    const auto resp_bytes = getRespBytes(resp_pdu);
    if (!resp_bytes)
      return nonstd::make_unexpected(resp_bytes.error());


    std::vector<bool> coils{};
    for (const auto& resp_byte : *resp_bytes) {
      for (std::size_t i = 0; i < 8; ++i) {
        if (coils.size() == number_of_points_) {
          break;
        }
        const bool bit_value = static_cast<bool>((resp_byte & (std::byte{1} << i)) >> i);
        coils.push_back(bit_value);
      }
    }
    return coils;
  }

  [[nodiscard]] std::byte getFunctionCode() const final {
    return std::byte{0x01};
  }

  [[nodiscard]] size_t expectedLength() const final {
    return number_of_points_ / 8 + (number_of_points_ % 8 != 0);
  }

  [[nodiscard]] nonstd::expected<std::string, std::error_code> serializeResponsePdu(const std::span<const std::byte> resp_pdu) const override {
    auto response = handleResponse(resp_pdu);
    if (!response) {
      return nonstd::make_unexpected(response.error());
    }
    return fmt::format("{}", fmt::join(*response, ", "));
  }

 private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

template<typename T>
class ReadRegisters : public ReadModbusFunction {
 public:
  ReadRegisters(const uint16_t transaction_id, const uint16_t starting_address, const uint16_t number_of_points)
      : ReadModbusFunction(transaction_id),
        starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(getFunctionCode());
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  [[nodiscard]] size_t expectedLength() const final {
    return number_of_points_*2;
  }

  [[nodiscard]] nonstd::expected<std::vector<T>, std::error_code> handleResponse(const std::span<const std::byte> resp_pdu) const {
    const auto resp_bytes = getRespBytes(resp_pdu);
    if (!resp_bytes)
      return nonstd::make_unexpected(resp_bytes.error());

    std::vector<T> holding_registers{};
    for (auto&& register_value : ranges::views::chunk(*resp_bytes, 2)) {
      auto span = std::span<const std::byte, 2>(register_value);
      holding_registers.push_back(convertFromBigEndian<T>(span));
    }
    return holding_registers;
  }

  [[nodiscard]] nonstd::expected<std::string, std::error_code> serializeResponsePdu(const std::span<const std::byte> resp_pdu) const override {
    auto response = handleResponse(resp_pdu);
    if (!response) {
      return nonstd::make_unexpected(response.error());
    }
    return fmt::format("{}", fmt::join(*response, ", "));
  }

 private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

template<typename T>
class ReadHoldingRegisters final : public ReadRegisters<T> {
 public:
  ReadHoldingRegisters(const uint16_t transaction_id, const uint16_t starting_address, const uint16_t number_of_points)
      : ReadRegisters<T>(transaction_id, starting_address, number_of_points) {
  }

  [[nodiscard]] std::byte getFunctionCode() const final {
    return std::byte{0x03};
  }
};

template<typename T>
class ReadInputRegisters final : public ReadRegisters<T> {
 public:
  ReadInputRegisters(const uint16_t transaction_id, const uint16_t starting_address, const uint16_t number_of_points)
      : ReadRegisters<T>(transaction_id, starting_address, number_of_points) {
  }

  [[nodiscard]] std::byte getFunctionCode() const final {
    return std::byte{0x04};
  }
};


}  // namespace org::apache::nifi::minifi::modbus
