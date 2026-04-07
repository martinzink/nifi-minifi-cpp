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

#include <string>

#include "api/core/FlowFile.h"
#include "api/core/SslContextService.h"
#include "api/utils/Ssl.h"
#include "minifi-c.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "nonstd/expected.hpp"

namespace org::apache::nifi::minifi::api::core {

class ProcessContext {
 public:
  virtual ~ProcessContext() noexcept = default;

  ProcessContext() = default;
  ProcessContext(const ProcessContext&) = delete;
  ProcessContext(ProcessContext&&) = delete;
  ProcessContext& operator=(const ProcessContext&) = delete;
  ProcessContext& operator=(ProcessContext&&) = delete;

  [[nodiscard]] virtual nonstd::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& prop,
      const FlowFile* ff) const = 0;
  [[nodiscard]] virtual nonstd::expected<MinifiControllerService*, std::error_code> getControllerService(std::string_view name,
      std::string_view type) const = 0;
  [[nodiscard]] virtual bool hasNonEmptyProperty(std::string_view name) const = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> getDynamicProperties() const = 0;

  [[nodiscard]] virtual nonstd::expected<utils::net::SslData, std::error_code> getSslData(std::string_view name) const = 0;

 protected:
  [[nodiscard]] virtual nonstd::expected<std::string, std::error_code> getProperty(std::string_view name, const FlowFile* flow_file) const = 0;
};

class CffiProcessContext : public ProcessContext {
 public:
  explicit CffiProcessContext(MinifiProcessContext* impl) : impl_(impl) {}

  [[nodiscard]] nonstd::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& property_reference,
      const FlowFile* flow_file) const override;
  [[nodiscard]] nonstd::expected<MinifiControllerService*, std::error_code> getControllerService(std::string_view name,
      std::string_view type) const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties() const override;
  [[nodiscard]] bool hasNonEmptyProperty(std::string_view name) const override;

  [[nodiscard]] nonstd::expected<utils::net::SslData, std::error_code> getSslData(std::string_view name) const override;

 protected:
  [[nodiscard]] nonstd::expected<std::string, std::error_code> getProperty(std::string_view name, const FlowFile* flow_file) const override;

 private:
  MinifiProcessContext* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
