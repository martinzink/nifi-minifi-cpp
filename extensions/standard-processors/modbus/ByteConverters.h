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
#include <span>
#include <string_view>

namespace org::apache::nifi::minifi::modbus {

inline std::array<std::byte, 2> convertToBigEndian(const uint16_t value) {
  std::array<std::byte, 2> result{};

  result[0] = static_cast<std::byte>((value >> 8) & 0xFF);
  result[1] = static_cast<std::byte>(value & 0xFF);

  return result;
}

template<class T>
T convertFromBigEndian(std::span<const std::byte, std::max(sizeof(T), sizeof(uint16_t))> bytes) = delete;

template<>
inline uint16_t convertFromBigEndian(std::span<const std::byte, 2> bytes) {
  uint16_t result = 0;

  result |= (static_cast<uint16_t>(bytes[0]) << 8);
  result |= static_cast<uint16_t>(bytes[1]);

  return result;
}

template<>
inline char convertFromBigEndian(std::span<const std::byte, 2> bytes) {
  return static_cast<char>(bytes[0]);  // TODO(mzink) which byte cula?
}

template<class Type>
bool type_matches(const std::string_view) {
  return false;
}

template<>
inline bool type_matches<uint16_t>(const std::string_view type_str) {
  return type_str == "UINT";
}

template<>
inline bool type_matches<bool>(const std::string_view type_str) {
  return type_str == "BOOL";
}
}  // namespace org::apache::nifi::minifi::modbus
