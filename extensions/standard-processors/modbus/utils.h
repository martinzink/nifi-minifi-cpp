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
#include <array>
#include <cstdint>

namespace org::apache::nifi::minifi::modbus {

inline std::array<std::byte, 2> convertToBigEndian(const uint16_t value) {
  std::array<std::byte, 2> result{};

  result[0] = static_cast<std::byte>((value >> 8) & 0xFF);
  result[1] = static_cast<std::byte>(value & 0xFF);

  return result;
}
}  // namespace org::apache::nifi::minifi::modbus
