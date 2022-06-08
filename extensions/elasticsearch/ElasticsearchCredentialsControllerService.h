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

#include "core/controller/ControllerService.h"
#include "client/HTTPClient.h"
#include "utils/HTTPClient.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

class ElasticsearchCredentialsControllerService : public core::controller::ControllerService {
 public:
  SMART_ENUM(CredType,
             (USE_XPACK, "Use XPack username and password"),
             (USE_API_KEY, "Use API Key"));

  EXTENSIONAPI static const core::Property CredentialsType;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;
  EXTENSIONAPI static const core::Property ApiKey;

  using ControllerService::ControllerService;

  void initialize() override;

  void yield() override {}

  bool isWorkAvailable() override {
    return false;
  }

  bool isRunning() override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable(core::controller::ControllerServiceProvider*) override;

  void authenticateClient(utils::HTTPClient& client);
 private:
  std::optional<std::string> password_;
  std::optional<std::string> username_;
  std::optional<std::string> api_key_;
  CredType cred_type_ = CredType::USE_API_KEY;
};
}  // org::apache::nifi::minifi::extensions::elasticsearch
