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
#pragma once



#include "ParsingErrors.h"
#include "StringUtils.h"
#include "TimeUtil.h"
#include "Literals.h"
#include "fmt/format.h"

namespace org::apache::nifi::minifi::parsing {
inline nonstd::expected<bool, std::error_code> parseBool(const std::string_view input) {
  if (utils::string::equalsIgnoreCase(input, "true")) { return true; }
  if (utils::string::equalsIgnoreCase(input, "false")) { return false; }

  return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
}

template<std::integral T>
nonstd::expected<T, std::error_code> parseIntegralMinMax(const std::string_view input, const T minimum, const T maximum) {
  const auto trimmed_input = utils::string::trim(input);
  T t{};
  const auto [ptr, ec] = std::from_chars(trimmed_input.data(), trimmed_input.data() + trimmed_input.size(), t);
  if (ec != std::errc()) {
    return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
  }
  if (ptr != trimmed_input.data() + trimmed_input.size()) {
    return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
  }

  if (t < minimum) { return nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum); }
  if (t > maximum) { return nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum); }
  return t;
}

template<std::integral T>
nonstd::expected<T, std::error_code> parseIntegral(const std::string_view input) {
  return parseIntegralMinMax<T>(input, std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
}

template<class TargetDuration>
nonstd::expected<TargetDuration, std::error_code> parseDurationMinMax(const std::string_view input, const TargetDuration minimum, const TargetDuration maximum) {
  auto duration = utils::timeutils::StringToDuration<std::chrono::milliseconds>(input);
  if (!duration.has_value()) { return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError); }

  if (*duration < minimum) { return nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum); }
  if (*duration > maximum) { return nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum); }

  return *duration;
}

// utils::andThen doesnt like default parameters
template<class TargetDuration = std::chrono::milliseconds>
nonstd::expected<TargetDuration, std::error_code> parseDuration(const std::string_view input) {
  return parseDurationMinMax<TargetDuration>(input, TargetDuration::min(), TargetDuration::max());
}

inline uint64_t getUnitMultiplier(const std::string_view unit_str) {
  static std::map<std::string, int64_t, std::less<>> unit_map{
        {"B", 1},
        {"K", 1_KB},
        {"M", 1_MB},
        {"G", 1_GB},
        {"T", 1_TB},
        {"P", 1_PB},
        {"KB", 1_KiB},
        {"MB", 1_MiB},
        {"GB", 1_GiB},
        {"TB", 1_TiB},
        {"PB", 1_PiB},
        {"KiB", 1_KiB},
        {"MiB", 1_MiB},
        {"GiB", 1_GiB},
        {"TiB", 1_TiB},
        {"PiB", 1_PiB},
    };
  const auto unit_multiplier = unit_map.find(unit_str);
  if (unit_multiplier != unit_map.end()) { return unit_multiplier->second; }


  return 1;
}

inline nonstd::expected<uint64_t, std::error_code> parseDataSizeMinMax(const std::string_view input, const uint64_t minimum, const uint64_t maximum) {
  const auto trimmed_input = utils::string::trim(input);
  const auto split_pos = trimmed_input.find_first_not_of("0123456789");

  if (split_pos == std::string_view::npos) {
    return parseIntegral<uint64_t>(trimmed_input);
  }

  const auto num_str = trimmed_input.substr(0, split_pos);
  const std::string unit_str = utils::string::toUpper(std::string{trimmed_input.substr(split_pos, trimmed_input.size() - split_pos)});

  nonstd::expected<uint64_t, std::error_code> num_part = parseIntegral<uint64_t>(num_str);
  if (!num_part) { return nonstd::make_unexpected(num_part.error()); }

  uint64_t result = *num_part * getUnitMultiplier(utils::string::trim(unit_str));
  if (result < minimum) { return nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum); }
  if (result > maximum) { return nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum); }

  return result;
}

inline nonstd::expected<uint64_t, std::error_code> parseDataSize(const std::string_view input) {
  return parseDataSizeMinMax(input, std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
}

inline nonstd::expected<uint32_t, std::error_code> parsePermissions(const std::string_view input) {
  uint32_t result = 0U;
  if (input.size() == 9U) {
    /* Probably rwxrwxrwx formatted */
    for (size_t i = 0; i < 3; i++) {
      if (input[i * 3] == 'r') {
        result |= 04 << ((2 - i) * 3);
      } else if (input[i * 3] != '-') {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
      if (input[i * 3 + 1] == 'w') {
        result |= 02 << ((2 - i) * 3);
      } else if (input[i * 3 + 1] != '-') {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
      if (input[i * 3 + 2] == 'x') {
        result |= 01 << ((2 - i) * 3);
      } else if (input[i * 3 + 2] != '-') {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
    }
  } else {
    /* Probably octal */
    try {
      size_t pos = 0U;
      result = std::stoul(std::string{input}, &pos, 8);
      if (pos != input.size()) {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
      if ((result & ~static_cast<uint32_t>(0777)) != 0U) {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
    } catch (...) {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
    }
  }
  return result;
}

template<typename T>
nonstd::expected<T, std::error_code> parseEnum(const std::string_view input) {
  std::optional<T> result = magic_enum::enum_cast<T>(input);
  if (!result) {
    return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
  }
  return *result;
}

}  // namespace org::apache::nifi::minifi::extension_utils