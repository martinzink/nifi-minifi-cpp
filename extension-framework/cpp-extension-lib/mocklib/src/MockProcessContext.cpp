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

#include "MockProcessContext.h"

#include "utils/expected.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::mock {

nonstd::expected<std::string, std::error_code> MockProcessContext::getProperty(std::string_view name, const api::core::FlowFile* flow_file) const {
  if (!properties_.contains(name)) {
    return nonstd::make_unexpected(make_error_code(core::PropertyErrorCode::PropertyNotSet));
  }
  return properties_.at(std::string(name));
}

nonstd::expected<std::string, std::error_code> MockProcessContext::getProperty(const minifi::core::PropertyReference& property_reference,
    const api::core::FlowFile* flow_file) const {
  auto property = getProperty(property_reference.name, flow_file);
  if (property)
    return property;
  if (property_reference.default_value) {
    return std::string{*property_reference.default_value};
  }
  return nonstd::make_unexpected(make_error_code(core::PropertyErrorCode::PropertyNotSet));
}


nonstd::expected<MinifiControllerService*, std::error_code> MockProcessContext::getControllerService(std::string_view controller_service_name,
    std::string_view controller_service_class) const {
  return nullptr;
}

std::map<std::string, std::string> MockProcessContext::getDynamicProperties() const {
  return {};
}

bool MockProcessContext::hasNonEmptyProperty(std::string_view name) const {
  return properties_.contains(name);
}

nonstd::expected<api::utils::net::SslData, std::error_code> MockProcessContext::getSslData(std::string_view name) const {
  return api::utils::net::SslData{};
}

}  // namespace org::apache::nifi::minifi::mock