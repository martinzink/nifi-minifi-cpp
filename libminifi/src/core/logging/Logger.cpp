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

LogBuilder::LogBuilder(BaseLogger *l, LOG_LEVEL level)
    : ignore(false),
      ptr(l),
      level_(level) {
  if (!l->should_log(level)) {
    setIgnore();
  }
}

LogBuilder::~LogBuilder() {
  if (!ignore)
    log_string(level_);
}

void LogBuilder::setIgnore() {
  ignore = true;
}

void LogBuilder::log_string(LOG_LEVEL level) const {
  ptr->log_string(level, str.str());
}


bool Logger::should_log(LOG_LEVEL level) {
  if (controller_ && !controller_->is_enabled())
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  return delegate_->should_log(mapToSpdLogLevel(level));
}

void Logger::log_string(LOG_LEVEL level, std::string str) {
  log(mapToSpdLogLevel(level), "{}", str);
}

LOG_LEVEL Logger::level() const {
  return mapFromSpdLogLevel(delegate_->level());
}

Logger::Logger(std::shared_ptr<spdlog::logger> delegate, std::shared_ptr<LoggerControl> controller)
    : delegate_(std::move(delegate)), controller_(std::move(controller)) {
}

Logger::Logger(std::shared_ptr<spdlog::logger> delegate)
    : delegate_(std::move(delegate)), controller_(nullptr) {
}

}  // namespace org::apache::nifi::minifi::core::logging
