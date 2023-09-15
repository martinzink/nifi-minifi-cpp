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
#include <mutex>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <iostream>
#include <vector>
#include <algorithm>

#include "spdlog/common.h"
#include "spdlog/logger.h"
#include "utils/gsl.h"
#include "utils/Enum.h"
#include "fmt/chrono.h"
#include "fmt/std.h"

namespace org::apache::nifi::minifi::core::logging {

inline constexpr size_t LOG_BUFFER_SIZE = 1024;

class LoggerControl {
 public:
  LoggerControl();

  [[nodiscard]] bool is_enabled() const;

  void setEnabled(bool status);

 protected:
  std::atomic<bool> is_enabled_;
};

template<typename Arg>
inline decltype(auto) conditional_stringify(Arg&& arg) {
  if constexpr (std::is_invocable_v<Arg>) {
    return std::forward<Arg>(arg)();
  } else {
    return std::forward<Arg>(arg);
  }
}

enum LOG_LEVEL {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  err = 4,
  critical = 5,
  off = 6
};

inline spdlog::level::level_enum mapToSpdLogLevel(LOG_LEVEL level) {
  switch (level) {
    case trace: return spdlog::level::trace;
    case debug: return spdlog::level::debug;
    case info: return spdlog::level::info;
    case warn: return spdlog::level::warn;
    case err: return spdlog::level::err;
    case critical: return spdlog::level::critical;
    case off: return spdlog::level::off;
    default:
      throw std::invalid_argument(fmt::format("Invalid LOG_LEVEL {}", magic_enum::enum_underlying(level)));
  }
}

inline LOG_LEVEL mapFromSpdLogLevel(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::trace: return LOG_LEVEL::trace;
    case spdlog::level::debug: return LOG_LEVEL::debug;
    case spdlog::level::info: return LOG_LEVEL::info;
    case spdlog::level::warn: return LOG_LEVEL::warn;
    case spdlog::level::err: return LOG_LEVEL::err;
    case spdlog::level::critical: return LOG_LEVEL::critical;
    case spdlog::level::off: return LOG_LEVEL::off;
    case spdlog::level::n_levels: return LOG_LEVEL::off;
    default:
      throw std::invalid_argument(fmt::format("Invalid spdlog::level::level_enum {}", magic_enum::enum_underlying(level)));
  }
}

class BaseLogger {
 public:
  virtual ~BaseLogger();

  virtual void log_string(LOG_LEVEL level, std::string str) = 0;
  virtual bool should_log(LOG_LEVEL level) = 0;
  virtual LOG_LEVEL level() const = 0;
};

class LogBuilder {
 public:
  LogBuilder(BaseLogger *l, LOG_LEVEL level);

  ~LogBuilder();

  void setIgnore();

  void log_string(LOG_LEVEL level) const;

  template<typename T>
  LogBuilder &operator<<(const T &o) {
    if (!ignore)
      str << o;
    return *this;
  }

  bool ignore;
  BaseLogger *ptr;
  std::stringstream str;
  LOG_LEVEL level_;
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
const auto map_args = overloaded {
    [](std::invocable<> auto&& f) { return std::invoke(std::forward<decltype(f)>(f)); },
    [](auto&& value) { return std::forward<decltype(value)>(value); }
};

class Logger : public BaseLogger {
 public:
  Logger(Logger const&) = delete;
  Logger& operator=(Logger const&) = delete;

  template<typename ...Ts>
  void log_with_level(LOG_LEVEL log_level, fmt::format_string<std::invoke_result_t<decltype(map_args), Ts>...> fmt, Ts&& ...args) {
    return log(mapToSpdLogLevel(log_level), std::move(fmt), std::forward<Ts>(args)...);
  }

  template<typename ...Ts>
  void log_critical(fmt::format_string<std::invoke_result_t<decltype(map_args), Ts>...> fmt, Ts&& ...args) {
    log(spdlog::level::critical, std::move(fmt), std::forward<Ts>(args)...);
  }

  template<typename ...Ts>
  void log_error(fmt::format_string<std::invoke_result_t<decltype(map_args), Ts>...> fmt, Ts&& ...args) {
    log(spdlog::level::err, std::move(fmt), std::forward<Ts>(args)...);
  }

  template<typename ...Ts>
  void log_warn(fmt::format_string<std::invoke_result_t<decltype(map_args), Ts>...> fmt, Ts&& ...args) {
    log(spdlog::level::warn, std::move(fmt), std::forward<Ts>(args)...);
  }

  template<typename ...Ts>
  void log_info(fmt::format_string<std::invoke_result_t<decltype(map_args), Ts>...> fmt, Ts&& ...args) {
    log(spdlog::level::info, std::move(fmt), std::forward<Ts>(args)...);
  }

  template<typename ...Ts>
  void log_debug(fmt::format_string<std::invoke_result_t<decltype(map_args), Ts>...> fmt, Ts&& ...args) {
    log(spdlog::level::debug, std::move(fmt), std::forward<Ts>(args)...);
  }

  template<typename ...Ts>
  void log_trace(fmt::format_string<std::invoke_result_t<decltype(map_args), Ts>...> fmt, Ts&& ...args) {
    log(spdlog::level::trace, std::move(fmt), std::forward<Ts>(args)...);
  }

  void set_max_log_size(int size) {
    max_log_size_ = size;
  }

  bool should_log(LOG_LEVEL level) override;
  void log_string(LOG_LEVEL level, std::string str) override;
  LOG_LEVEL level() const override;

  virtual std::optional<std::string> get_id() = 0;

 protected:
  Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller);

  Logger(std::shared_ptr<spdlog::logger> delegate); // NOLINT

  std::shared_ptr<spdlog::logger> delegate_;
  std::shared_ptr<LoggerControl> controller_;

  std::mutex mutex_;

 private:
  std::string trimToMaxSizeAndAddId(std::string&& my_string) {
    auto max_log_size = max_log_size_.load();
    if (max_log_size >= 0 && my_string.size() > gsl::narrow<size_t>(max_log_size))
      my_string = my_string.substr(0, max_log_size);
    if (auto id = get_id()) {
      my_string += *id;
    }
    return my_string;
  }

  template<typename ...T>
  std::string stringify(fmt::format_string<T...> fmt, T&&... args) {
    auto log_message = fmt::format(fmt, std::forward<T>(args)...);
    return trimToMaxSizeAndAddId(std::move(log_message));
  }

  template<typename ...T>
  inline void log(spdlog::level::level_enum level, fmt::format_string<std::invoke_result_t<decltype(map_args), T>...> fmt, T&& ...args) {
    if (controller_ && !controller_->is_enabled())
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!delegate_->should_log(level)) {
      return;
    }
    delegate_->log(level, stringify(fmt, map_args(std::forward<T>(args))...));
  }

  std::atomic<int> max_log_size_{LOG_BUFFER_SIZE};
};

#define LOG_TRACE(x) LogBuilder((x).get(), org::apache::nifi::minifi::core::logging::LOG_LEVEL::trace)

#define LOG_DEBUG(x) LogBuilder((x).get(), org::apache::nifi::minifi::core::logging::LOG_LEVEL::debug)

#define LOG_INFO(x) LogBuilder((x).get(), org::apache::nifi::minifi::core::logging::LOG_LEVEL::info)

#define LOG_WARN(x) LogBuilder((x).get(), org::apache::nifi::minifi::core::logging::LOG_LEVEL::warn)

#define LOG_ERROR(x) LogBuilder((x).get(), org::apache::nifi::minifi::core::logging::LOG_LEVEL::err)

#define LOG_CRITICAL(x) LogBuilder((x).get(), org::apache::nifi::minifi::core::logging::LOG_LEVEL::err)

}  // namespace org::apache::nifi::minifi::core::logging
