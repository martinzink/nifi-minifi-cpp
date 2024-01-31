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

#include "modbus/ReadModbusFunctions.h"
#include "Catch.h"
namespace org::apache::nifi::minifi::modbus::test {

template <typename... Bytes>
std::vector<std::byte> createByteVector(Bytes... bytes) {
  return {static_cast<std::byte>(bytes)...};
}

TEST_CASE("ReadCoilStatus") {
  const auto read_coil_status = ReadCoilStatus(280, 19, 19);
  {
    {
      CHECK(read_coil_status.rawPdu() == createByteVector(0x01, 0x00, 0x13, 0x00, 0x13));
      CHECK(read_coil_status.requestBytes() == createByteVector(0x01, 0x18, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x13, 0x00, 0x13));
    }

    auto serialized_response = read_coil_status.serializeResponsePdu(createByteVector(0x01, 0x03, 0xCD, 0x6B, 0x05));
    REQUIRE(serialized_response.has_value());
    CHECK(*serialized_response == "true, false, true, true, false, false, true, true, true, true, false, true, false, true, true, false, true, false, true");
  }

  {
    auto shorter_than_expected_resp = read_coil_status.handleResponse(createByteVector(0x01, 0x02, 0xCD, 0x6B));
    REQUIRE(!shorter_than_expected_resp);
    CHECK(shorter_than_expected_resp.error() == modbus::ModbusExceptionCode::InvalidResponse);
  }


  {
    auto longer_than_expected_resp = read_coil_status.handleResponse(createByteVector(0x01, 0x04, 0xCD, 0x6B, 0x05, 0x07));
    REQUIRE(!longer_than_expected_resp);
    CHECK(longer_than_expected_resp.error() == modbus::ModbusExceptionCode::InvalidResponse);
  }

  {
    auto mismatching_size_resp = read_coil_status.handleResponse(createByteVector(0x01, 0x03, 0xCD, 0x6B, 0x05, 0x07));
    REQUIRE(!mismatching_size_resp);
    CHECK(mismatching_size_resp.error() == modbus::ModbusExceptionCode::InvalidResponse);
  }
}

TEST_CASE("ReadHoldingRegisters") {
  {
    const auto read_holding_registers = ReadHoldingRegisters<uint16_t>(0, 5, 3);
    {
      CHECK(read_holding_registers.rawPdu() == createByteVector(0x03, 0x00, 0x05, 0x00, 0x03));
    }

    auto serialized_response = read_holding_registers.serializeResponsePdu(createByteVector(0x03, 0x06, 0x3A, 0x98, 0x13, 0x88, 0x00, 0xC8));
    REQUIRE(serialized_response.has_value());
    CHECK(*serialized_response == "15000, 5000, 200");
  }
}

TEST_CASE("ReadInputRegisters") {
  {
    const auto read_input_registers = ReadInputRegisters<uint16_t>(12, 5, 3);
    {
      CHECK(read_input_registers.rawPdu() == createByteVector(0x04, 0x00, 0x05, 0x00, 0x03));
    }
    auto serialized_response = read_input_registers.serializeResponsePdu(createByteVector(0x04, 0x06, 0x3A, 0x98, 0x13, 0x88, 0x00, 0xC8));
    REQUIRE(serialized_response.has_value());
    CHECK(*serialized_response == "15000, 5000, 200");
  }
}

}  // namespace org::apache::nifi::minifi::modbus::test
