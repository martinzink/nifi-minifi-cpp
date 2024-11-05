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

#include <string>
#include <system_error>
#include "magic_enum.hpp"

namespace org::apache::nifi::minifi::core {

enum class PropertyErrorCode : std::underlying_type_t<std::byte> {
  MissingOptional,
  MissingRequired,
  ValidationFailed,
  ConversionError,
  InvalidProperty,
};


struct PropertyErrorCategory final : std::error_category {
  [[nodiscard]] const char* name() const noexcept override {
    return "property error";
  }

  [[nodiscard]] std::string message(int ev) const override {
    const auto property_error_code = static_cast<PropertyErrorCode>(ev);
    auto property_error_code_str = std::string{magic_enum::enum_name<PropertyErrorCode>(property_error_code)};
    if (property_error_code_str.empty()) {
      return "UNKNOWN ERROR";
    }
    return property_error_code_str;
  }
};

inline const PropertyErrorCategory& property_category() noexcept {
  static PropertyErrorCategory category;
  return category;
};

inline std::error_code make_error_code(PropertyErrorCode c) {
  return {static_cast<int>(c), property_category()};
}

}  // namespace org::apache::nifi::minifi::core

template <>
struct std::is_error_code_enum<org::apache::nifi::minifi::core::PropertyErrorCode> : std::true_type {};
