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
#include <filesystem>
#include <string>
#include <memory>

#include "core/controller/ControllerService.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::extensions::smb {

class SmbConnectionControllerService : public core::controller::ControllerService {
 public:
  EXTENSIONAPI static constexpr const char* Description = "SMB Connection Controller Service";

  EXTENSIONAPI static const core::Property Hostname;
  EXTENSIONAPI static const core::Property Share;
  EXTENSIONAPI static const core::Property Domain;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;

  static auto properties() {
    return std::array{
        Hostname,
        Share,
        Domain,
        Username,
        Password
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  using ControllerService::ControllerService;

  void initialize() override;

  void yield() override {
  }

  bool isWorkAvailable() override {
    return false;
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override;

  nonstd::expected<void, std::error_code> connect();
  nonstd::expected<void, std::error_code> disconnect();
  nonstd::expected<bool, std::error_code> isConnected();

  const std::string& getPath() const { return server_path_; }

 private:
  struct Credentials {
    std::string username;
    std::string password;
  };

  std::optional<Credentials> credentials_;
  std::string server_path_;
  NETRESOURCEA net_resource_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SmbConnectionControllerService>::getLogger(uuid_);
};
}  // namespace org::apache::nifi::minifi::extensions::smb
