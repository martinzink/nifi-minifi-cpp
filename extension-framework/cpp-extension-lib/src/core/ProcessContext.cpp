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

#include "api/core/ProcessContext.h"

#include "api/core/FlowFile.h"
#include "api/utils/minifi-c-utils.h"

namespace org::apache::nifi::minifi::api::core {

nonstd::expected<std::string, std::error_code> CffiProcessContext::getProperty(const minifi::core::PropertyReference& property_reference,
    const FlowFile* flow_file) const {
  return getProperty(property_reference.name, flow_file);
}

nonstd::expected<std::string, std::error_code> CffiProcessContext::getProperty(std::string_view name, const FlowFile* flow_file) const {
  std::optional<std::string> value;
  const MinifiStatus status = MinifiProcessContextGetProperty(
      impl_,
      utils::minifiStringView(name),
      flow_file ? flow_file->get() : MINIFI_NULL,
      [](void* data, const MinifiStringView result) { (*static_cast<std::optional<std::string>*>(data)) = std::string(result.data, result.length); },
      &value);

  if (!value) { return nonstd::make_unexpected(utils::make_error_code(status)); }
  return value.value();
}

bool CffiProcessContext::hasNonEmptyProperty(std::string_view name) const {
  return MinifiProcessContextHasNonEmptyProperty(impl_, utils::minifiStringView(name));
}

nonstd::expected<MinifiControllerService*, std::error_code> CffiProcessContext::getControllerService(const std::string_view name,
    const std::string_view type) const {
  MinifiControllerService* controller_service = nullptr;
  if (const MinifiStatus status = MinifiProcessContextGetControllerService(impl_,
          utils::minifiStringView(name),
          utils::minifiStringView(type),
          &controller_service);
      status != MINIFI_STATUS_SUCCESS) {
    return nonstd::make_unexpected(utils::make_error_code(status));
  }
  return controller_service;
}

std::map<std::string, std::string> CffiProcessContext::getDynamicProperties() const {
  std::map<std::string, std::string> result;
  MinifiProcessContextGetDynamicProperties(
      impl_,
      [](void* user_ctx, const MinifiStringView key, const MinifiStringView value) {
        static_cast<std::map<std::string, std::string>*>(user_ctx)->emplace(utils::toString(key), utils::toString(value));
      },
      &result);
  return result;
}

nonstd::expected<utils::net::SslData, std::error_code> CffiProcessContext::getSslData(const std::string_view name) const {
  MinifiSslContextService* minifi_ssl_service = nullptr;
  if (const auto status = MinifiProcessContextGetSslContextService(impl_, utils::minifiStringView(name), &minifi_ssl_service);
      status != MINIFI_STATUS_SUCCESS) {
    return nonstd::make_unexpected(utils::make_error_code(status));
  }

  CffiSslContextService ssl_service{minifi_ssl_service};
  auto ca_location = ssl_service.caLocation();
  if (!ca_location) { return nonstd::make_unexpected(ca_location.error()); }

  auto cert_location = ssl_service.certLocation();
  if (!cert_location) { return nonstd::make_unexpected(cert_location.error()); }

  auto key_location = ssl_service.keyLocation();
  if (!key_location) { return nonstd::make_unexpected(key_location.error()); }

  auto passphrase = ssl_service.keyPassword();
  if (!passphrase) { return nonstd::make_unexpected(passphrase.error()); }

  auto ssl_data = utils::net::SslData{
      .ca_loc = *ca_location,
      .cert_loc = *cert_location,
      .key_loc = *key_location,
      .key_pw = *passphrase,
  };

  return ssl_data;
}

}  // namespace org::apache::nifi::minifi::api::core
