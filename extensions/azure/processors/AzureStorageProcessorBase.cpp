/**
 * @file AzureStorageProcessorBase.cpp
 * AzureStorageProcessorBase class implementation
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

#include "AzureStorageProcessorBase.h"

#include <memory>
#include <string>

#include "controllerservices/AzureStorageCredentialsService.h"

namespace org::apache::nifi::minifi::azure::processors {

const core::Property AzureStorageProcessorBase::AzureStorageCredentialsService(
  core::PropertyBuilder::createProperty("Azure Storage Credentials Service")
    ->withDescription("Name of the Azure Storage Credentials Service used to retrieve the connection string from.")
    ->build());

std::string AzureStorageProcessorBase::getConnectionStringFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const {
  std::string service_name;
  if (!context->getProperty(AzureStorageCredentialsService.getName(), service_name) || service_name.empty()) {
    return "";
  }

  std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(service_name);
  if (nullptr == service) {
    logger_->log_error("Azure Storage credentials service with name: '%s' could not be found", service_name);
    return "";
  }

  auto azure_credentials_service = std::dynamic_pointer_cast<minifi::azure::controllers::AzureStorageCredentialsService>(service);
  if (!azure_credentials_service) {
    logger_->log_error("Controller service with name: '%s' is not an Azure Storage credentials service", service_name);
    return "";
  }

  return azure_credentials_service->getConnectionString();
}

}  // namespace org::apache::nifi::minifi::azure::processors