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
#include "core/PropertyDefinitionBuilder.h"
#include "KafkaClientControllerService.h"

namespace org::apache::nifi::minifi::kafka {

class CommitKafkaPoC : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "CommitKafkaPoC";
  EXTENSIONAPI static constexpr auto KafkaClient =
    core::PropertyDefinitionBuilder<>::createProperty("Kafka Client Controller Service")
        .withDescription("Kafka Client Controller Service")
        .isRequired(true)
        .withAllowedTypes<KafkaClientControllerService>()
        .build();

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({KafkaClient});

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  using ProcessorImpl::ProcessorImpl;

private:
  std::shared_ptr<KafkaClientControllerService> kafka_client_controller_service_;
};

}  // namespace org::apache::nifi::minifi::kafka
