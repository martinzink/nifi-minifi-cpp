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
#include "range/v3/view/subrange.hpp"
#include "range/v3/view/chunk.hpp"

#include "utils/expected.h"

namespace org::apache::nifi::minifi::modbus {

class ReadModbusFunction {
 public:
  ReadModbusFunction() = default;
  ReadModbusFunction(const ReadModbusFunction&) = delete;
  ReadModbusFunction(ReadModbusFunction&&) = delete;
  ReadModbusFunction& operator=(ReadModbusFunction&&) = delete;
  ReadModbusFunction& operator=(const ReadModbusFunction&) = delete;

  virtual ~ReadModbusFunction() = default;

  [[nodiscard]] auto getBytes(const std::vector<std::byte>& resp_pdu) const -> nonstd::expected<ranges::subrange<std::vector<std::byte>::const_iterator>, std::error_code> {
    if (resp_pdu.size() < 2) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    if (const auto resp_function_code = resp_pdu.front(); resp_function_code != getFunctionCode()) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    const uint8_t resp_byte_count = static_cast<uint8_t>(resp_pdu.at(1));
    if (resp_pdu.size() != resp_byte_count + 2) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    if (resp_byte_count != expectedLength()) {
      return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
    }

    return ranges::subrange(resp_pdu.begin() + 2, resp_pdu.end());
  }

  [[nodiscard]] virtual std::byte getFunctionCode() const = 0;
  [[nodiscard]] virtual std::vector<std::byte> rawPdu() const = 0;
  [[nodiscard]] virtual size_t expectedLength() const = 0;
};

class ReadCoilStatus final : public ReadModbusFunction {
 public:
  ReadCoilStatus(const uint16_t starting_address, const uint16_t number_of_points)
      : starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(function_code_);
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  [[nodiscard]] nonstd::expected<std::vector<bool>, std::error_code> handleResponse(const std::vector<std::byte>& resp_pdu) const {
    const auto resp_bytes = getBytes(resp_pdu);
    if (!resp_bytes)
      return nonstd::make_unexpected(resp_bytes.error());


    std::vector<bool> coils{};
    for (const auto& resp_byte : *resp_bytes) {
      for (std::size_t i = 0; i < 8; ++i) {
        if (coils.size() == number_of_points_) {
          break;
        }
        const bool bit_value = static_cast<bool>((resp_byte & std::byte{unsigned{1} << i}) >> i);
        coils.push_back(bit_value);
      }
    }
    return coils;
  }

  [[nodiscard]] std::byte getFunctionCode() const final {
    return function_code_;
  }

  [[nodiscard]] size_t expectedLength() const final {
    return number_of_points_ / 8 + (number_of_points_ % 8 != 0);
  }


  static constexpr std::byte function_code_{0x01};

 private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

class ReadHoldingRegisters final : public ReadModbusFunction {
 public:
  ReadHoldingRegisters(const uint16_t starting_address, const uint16_t number_of_points)
      : starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(function_code_);
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  static constexpr std::byte function_code_{0x03};

  [[nodiscard]] std::byte getFunctionCode() const final {
    return function_code_;
  }

  [[nodiscard]] size_t expectedLength() const final {
    return number_of_points_*2;
  }

  template<class T>
  [[nodiscard]] nonstd::expected<std::vector<T>, std::error_code> handleResponse(const std::vector<std::byte>& resp_pdu) const {
    const auto resp_bytes = getBytes(resp_pdu);
    if (!resp_bytes)
      return nonstd::make_unexpected(resp_bytes.error());

    std::vector<T> holding_registers{};
    for (const auto& register_value : ranges::views::chunk(*resp_bytes, 2)) {
      holding_registers.push_back(convertFromBigEndian<T>({register_value[0], register_value[1]}));
    }
    return holding_registers;
  }

 private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

class ReadInputRegisters final : public ReadModbusFunction {
 public:
  ReadInputRegisters(const uint16_t starting_address, const uint16_t number_of_points)
      : starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(function_code_);
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  static constexpr std::byte function_code_{0x04};

  [[nodiscard]] std::byte getFunctionCode() const final {
    return function_code_;
  }

  [[nodiscard]] size_t expectedLength() const final {
    return number_of_points_*2;
  }

  template<class T>
  [[nodiscard]] nonstd::expected<std::vector<T>, std::error_code> handleResponse(const std::vector<std::byte>& resp_pdu) const {
    const auto resp_bytes = getBytes(resp_pdu);
    if (!resp_bytes)
      return nonstd::make_unexpected(resp_bytes.error());

    std::vector<T> holding_registers{};
    for (const auto& register_value : ranges::views::chunk(*resp_bytes, 2)) {
      holding_registers.push_back(convertFromBigEndian<T>({register_value[0], register_value[1]}));
    }
    return holding_registers;
  }

 private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

}  // namespace org::apache::nifi::minifi::modbus
