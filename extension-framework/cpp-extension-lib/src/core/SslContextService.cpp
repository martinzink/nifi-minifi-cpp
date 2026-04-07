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

#include "api/core/SslContextService.h"
#include "api/utils/minifi-c-utils.h"

namespace org::apache::nifi::minifi::api::core {

nonstd::expected<std::filesystem::path, std::error_code> CffiSslContextService::caLocation() const {
  std::string result;
  const auto status = MinifiSslContextServiceGetCACertificate(impl_, [] (void* user_ctx, const MinifiStringView value) {
    *static_cast<std::string*>(user_ctx) = utils::toString(value);
  }, &result);
  if (MINIFI_STATUS_SUCCESS != status) {
    return nonstd::make_unexpected(utils::make_error_code(status));
  }
  return result;
}

nonstd::expected<std::filesystem::path, std::error_code> CffiSslContextService::certLocation() const {
  std::string result;
  const auto status = MinifiSslContextServiceGetCertificateFile(impl_, [] (void* user_ctx, const MinifiStringView value) {
    *static_cast<std::string*>(user_ctx) = utils::toString(value);
  }, &result);
  if (MINIFI_STATUS_SUCCESS != status) {
    return nonstd::make_unexpected(utils::make_error_code(status));
  }
  return result;
}

nonstd::expected<std::filesystem::path, std::error_code> CffiSslContextService::keyLocation() const {
  std::string result;
  const auto status = MinifiSslContextServiceGetPrivateKeyFile(impl_, [] (void* user_ctx, const MinifiStringView value) {
    *static_cast<std::string*>(user_ctx) = utils::toString(value);
  }, &result);
  if (MINIFI_STATUS_SUCCESS != status) {
    return nonstd::make_unexpected(utils::make_error_code(status));
  }
  return result;
}

nonstd::expected<std::string, std::error_code> CffiSslContextService::keyPassword() const {
  std::string result;
  const auto status = MinifiSslContextServiceGetPassphrase(impl_, [] (void* user_ctx, const MinifiStringView value) {
    *static_cast<std::string*>(user_ctx) = utils::toString(value);
  }, &result);
  if (MINIFI_STATUS_SUCCESS != status) {
    return nonstd::make_unexpected(utils::make_error_code(status));
  }
  return result;
}


}  // namespace org::apache::nifi::minifi::api::core
