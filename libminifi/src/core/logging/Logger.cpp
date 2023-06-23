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

#include "core/logging/Logger.h"

#include <mutex>
#include <memory>
#include <sstream>
#include <iostream>

namespace org::apache::nifi::minifi::core::logging {

LoggerControl::LoggerControl()
    : is_enabled_(true) {
}

bool LoggerControl::is_enabled() const {
  return is_enabled_;
}

void LoggerControl::setEnabled(bool status) {
  is_enabled_ = status;
}


BaseLogger::~BaseLogger() = default;

bool BaseLogger::should_log(const LogLevelOption& /*level*/) {
  return true;
}

LogBuilder::LogBuilder(BaseLogger *l, LogLevelOption level)
    : ignore(false),
      ptr(l),
      level(level) {
  if (!l->should_log(level)) {
    setIgnore();
  }
}

LogBuilder::~LogBuilder() {
  if (!ignore)
    log_string(level);
}

void LogBuilder::setIgnore() {
  ignore = true;
}

void LogBuilder::log_string(LogLevelOption level) const {
  ptr->log_string(level, str.str());
}


bool Logger::should_log(const LogLevelOption &level) {
  if (controller_ && !controller_->is_enabled())
    return false;
  spdlog::level::level_enum logger_level = spdlog::level::level_enum::info;
  switch (level.value()) {
    case LogLevelOption::CRITICAL:
      logger_level = spdlog::level::level_enum::critical;
      break;
    case LogLevelOption::ERR:
      logger_level = spdlog::level::level_enum::err;
      break;
    case LogLevelOption::INFO:
      logger_level = spdlog::level::level_enum::info;
      break;
    case LogLevelOption::DEBUG:
      logger_level = spdlog::level::level_enum::debug;
      break;
    case LogLevelOption::OFF:
      logger_level = spdlog::level::level_enum::off;
      break;
    case LogLevelOption::TRACE:
      logger_level = spdlog::level::level_enum::trace;
      break;
    case LogLevelOption::WARN:
      logger_level = spdlog::level::level_enum::warn;
      break;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  return delegate_->should_log(logger_level);
}

void Logger::log_string(LogLevelOption level, std::string str) {
  switch (level.value()) {
    case LogLevelOption::CRITICAL:
      log_critical(str.c_str());
      break;
    case LogLevelOption::ERR:
      log_error(str.c_str());
      break;
    case LogLevelOption::INFO:
      log_info(str.c_str());
      break;
    case LogLevelOption::DEBUG:
      log_debug(str.c_str());
      break;
    case LogLevelOption::TRACE:
      log_trace(str.c_str());
      break;
    case LogLevelOption::WARN:
      log_warn(str.c_str());
      break;
    case LogLevelOption::OFF:
      break;
  }
}

Logger::Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller)
    : delegate_(std::move(delegate)), controller_(std::move(controller)) {
}

Logger::Logger(std::shared_ptr<spdlog::logger> delegate)
    : delegate_(std::move(delegate)), controller_(nullptr) {
}

}  // namespace org::apache::nifi::minifi::core::logging
