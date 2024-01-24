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
#include "modbus/utils.h"
#include "range/v3/algorithm/copy.hpp"

namespace org::apache::nifi::minifi::modbus {


struct ApplicationProtocolHeader {
  uint16_t transaction_identifier{};
  uint16_t protocol_identifier{};
  uint16_t length{};
  uint16_t unit_id{};
};

inline std::vector<std::byte> readCoilStatusPdu(const uint16_t starting_address, const uint16_t number_of_points) {
  std::vector<std::byte> result;
  result.push_back(std::byte{0x01});
  ranges::copy(convertToBigEndian(starting_address), std::back_inserter(result));
  ranges::copy(convertToBigEndian(number_of_points), std::back_inserter(result));
  return result;
}

inline std::vector<std::byte> readHoldingRegistersPdu(const uint16_t starting_address, const uint16_t number_of_points) {
  std::vector<std::byte> result;
  result.push_back(std::byte{0x01});
  ranges::copy(convertToBigEndian(starting_address), std::back_inserter(result));
  ranges::copy(convertToBigEndian(number_of_points), std::back_inserter(result));
  return result;
}

inline std::vector<std::byte> readCoilStatusPdu(const uint16_t starting_address, const uint16_t number_of_points) {
  std::vector<std::byte> result;
  result.push_back(std::byte{0x01});
  ranges::copy(convertToBigEndian(starting_address), std::back_inserter(result));
  ranges::copy(convertToBigEndian(number_of_points), std::back_inserter(result));
  return result;
}

class ModbusFunction {
 public:
  ModbusFunction() = default;
  ModbusFunction(const ModbusFunction&) = delete;
  ModbusFunction(ModbusFunction&&) = delete;
  ModbusFunction& operator=(ModbusFunction&&) = delete;
  ModbusFunction& operator=(const ModbusFunction&) = delete;

  virtual ~ModbusFunction() = default;

  [[nodiscard]] virtual std::vector<std::byte> rawPdu() const = 0;
};

class ReadCoilStatus final : public ModbusFunction {
 public:
  ReadCoilStatus(const uint16_t starting_address, const uint16_t number_of_points)
      : starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(function_code);
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  static constexpr std::byte function_code{0x01};

 private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

class ReadHoldingRegisters final : public ModbusFunction {
 public:
  ReadHoldingRegisters(const uint16_t starting_address, const uint16_t number_of_points)
      : starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(function_code);
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  static constexpr std::byte function_code{0x03};

private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

class ReadInputRegisters final : public ModbusFunction {
public:
  ReadInputRegisters(const uint16_t starting_address, const uint16_t number_of_points)
      : starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(function_code);
    ranges::copy(convertToBigEndian(starting_address_), std::back_inserter(result));
    ranges::copy(convertToBigEndian(number_of_points_), std::back_inserter(result));
    return result;
  }

  static constexpr std::byte function_code{0x04};

private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

class ReportSlaveId final : public ModbusFunction {
public:
  ReportSlaveId() = default;

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    return {function_code};
  }

  static constexpr std::byte function_code{0x11};
};





}  // namespace org::apache::nifi::minifi::modbus
