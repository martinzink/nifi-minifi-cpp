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

#include <filesystem>
#include "utils/expected.h"

#include "minifi-c.h"

namespace org::apache::nifi::minifi::api::core {

class SslContextService {
 public:
  virtual ~SslContextService() = default;

  [[nodiscard]] virtual nonstd::expected<std::filesystem::path, std::error_code> caLocation() const = 0;
  [[nodiscard]] virtual nonstd::expected<std::filesystem::path, std::error_code> certLocation() const = 0;
  [[nodiscard]] virtual nonstd::expected<std::filesystem::path, std::error_code> keyLocation() const = 0;
  [[nodiscard]] virtual nonstd::expected<std::string, std::error_code> keyPassword() const = 0;
};

class CffiSslContextService : public SslContextService {
 public:
  explicit CffiSslContextService(MinifiSslContextService* impl) : impl_(impl) {};

  [[nodiscard]] nonstd::expected<std::filesystem::path, std::error_code> caLocation() const override;
  [[nodiscard]] nonstd::expected<std::filesystem::path, std::error_code> certLocation() const override;
  [[nodiscard]] nonstd::expected<std::filesystem::path, std::error_code> keyLocation() const override;
  [[nodiscard]] nonstd::expected<std::string, std::error_code> keyPassword() const override;

 private:
  MinifiSslContextService* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
