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

#include "modbus/modbus.h"
#include "Catch.h"
namespace org::apache::nifi::minifi::modbus::test {

template <typename... Bytes>
std::vector<std::byte> createByteVector(Bytes... bytes) {
  return {static_cast<std::byte>(bytes)...};
}

TEST_CASE("Modbus Request") {
  CHECK(ReadCoilStatus(0, 4).rawPdu() == createByteVector(0x01, 0x00, 0x00, 0x00, 0x04));
  CHECK(ReadHoldingRegisters(5, 3).rawPdu() == createByteVector(0x03, 0x00, 0x05, 0x00, 0x03));
  CHECK(ReadInputRegisters(2, 2).rawPdu() == createByteVector(0x04, 0x00, 0x02, 0x00, 0x02));
  CHECK(ReportSlaveId().rawPdu() == createByteVector(0x11));
}

}  // namespace org::apache::nifi::minifi::modbus::test
