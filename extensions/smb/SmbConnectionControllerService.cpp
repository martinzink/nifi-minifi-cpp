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

#include "SmbConnectionControllerService.hpp"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "utils/OsUtils.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::extensions::smb {

const core::Property SmbConnectionControllerService::Hostname(
    core::PropertyBuilder::createProperty("Hostname")
        ->withDescription("The network host to which files should be written.")
        ->isRequired(true)
        ->build());

const core::Property SmbConnectionControllerService::Share(
    core::PropertyBuilder::createProperty("Share")
        ->withDescription(R"(The network share to which files should be written. This is the "first folder" after the hostname: \\hostname\[share]\dir1\dir2)")
        ->isRequired(true)
        ->build());

const core::Property SmbConnectionControllerService::Domain(
    core::PropertyBuilder::createProperty("Domain")
        ->withDescription("The domain used for authentication. Optional, in most cases username and password is sufficient.")
        ->isRequired(false)
        ->build());

const core::Property SmbConnectionControllerService::Username(
    core::PropertyBuilder::createProperty("Username")
        ->withDescription("The username used for authentication. If no username is set then anonymous authentication is attempted.")
        ->isRequired(false)
        ->build());

const core::Property SmbConnectionControllerService::Password(
    core::PropertyBuilder::createProperty("Password")
        ->withDescription("The password used for authentication. Required if Username is set.")
        ->isRequired(false)
        ->build());


void SmbConnectionControllerService::initialize() {
  setSupportedProperties(SmbConnectionControllerService::properties());
}

void SmbConnectionControllerService::onEnable()  {
  std::string hostname;
  std::string share;

  if (!getProperty(Hostname.getName(), hostname))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing hostname");

  if (!getProperty(Share.getName(), share))
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing share");


  server_path_ = "\\\\" + hostname + "\\" + share;

  auto password = getProperty(Password.getName());
  auto username = getProperty(Username.getName());

  if (password.has_value() != username.has_value())
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either a username and password must be provided, or neither of them should be provided.");

  if (username.has_value())
    credentials_.emplace(Credentials{.username = *username, .password = *password});
  else
    credentials_.reset();

  ZeroMemory(&net_resource_, sizeof(net_resource_));
  net_resource_.dwType = RESOURCETYPE_DISK;
  net_resource_.lpLocalName = nullptr;
  net_resource_.lpRemoteName = server_path_.data();
  net_resource_.lpProvider = nullptr;
}

nonstd::expected<void, std::error_code> SmbConnectionControllerService::connect() {
  auto connection_result = WNetAddConnection2A(&net_resource_,
      credentials_ ? credentials_->password.c_str() : nullptr,
      credentials_ ? credentials_->username.c_str() : nullptr,
      CONNECT_TEMPORARY);
  if (connection_result == NO_ERROR)
    return {};

  return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(connection_result));
}

nonstd::expected<void, std::error_code> SmbConnectionControllerService::disconnect() {
  auto disconnection_result = WNetCancelConnection2A(server_path_.c_str(), 0, true);
  if (disconnection_result == NO_ERROR)
    return {};

  return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(disconnection_result));
}

nonstd::expected<bool, std::error_code> SmbConnectionControllerService::isConnected() {
  return false;
}

REGISTER_RESOURCE(SmbConnectionControllerService, ControllerService);

}  // namespace org::apache::nifi::minifi::extensions::smb
