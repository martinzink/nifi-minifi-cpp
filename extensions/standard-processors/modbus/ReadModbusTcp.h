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
#include "core/Processor.h"
#include "utils/net/AsioCoro.h"

#include <PropertyDefinitionBuilder.h>
#include <logging/LoggerFactory.h>

namespace org::apache::nifi::minifi::modbus {
enum class AddressAccessStrategy {
  AddressFromProperty,
  AddressFromText,
  AddressFromFile
};


}  // namespace org::apache::nifi::minifi::modbus


namespace org::apache::nifi::minifi::modbus {

class ReadModbusTcp : public core::Processor {
 public:
  explicit ReadModbusTcp(const std::string_view name, const utils::Identifier& uuid = {})
    : Processor(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Processor able to read data from industrial PLCs using Modbus TCP/IP";

  EXTENSIONAPI static constexpr auto Hostname = core::PropertyDefinitionBuilder<>::createProperty("Hostname")
      .withDescription("The ip address or hostname of the destination.")
      .withDefaultValue("localhost")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();

  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withDescription("The port or service on the destination.")
      .withDefaultValue("502")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();

  EXTENSIONAPI static constexpr auto EndpointList = core::PropertyDefinitionBuilder<>::createProperty("Endpoint List")
      .withDescription("A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.")
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Timeout = core::PropertyDefinitionBuilder<>::createProperty("Timeout")
    .withDescription("Request timeout")
    .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
    .withDefaultValue("1s")
    .isRequired(true)
    .supportsExpressionLanguage(true)
    .build();

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 8>{
    Hostname,
    Port,
    Timeout,
  };

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully processed"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "An error occurred processing"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = true;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() final;

 private:
  std::unordered_map<std::string, std::string> getAddressMap(const core::FlowFile& flow_file) const;
  std::shared_ptr<core::FlowFile> getFlowFile(core::ProcessSession& session) const;

  asio::io_context io_context_;
  std::chrono::milliseconds timeout_duration_ = std::chrono::seconds(15);
  std::optional<asio::ssl::context> ssl_context_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ReadModbusTcp>::getLogger(uuid_);
};
}
