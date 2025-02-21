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

#include <array>
#include "core/controller/ControllerService.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/ProcessSession.h"
#include "../rdkafka_utils.h"

namespace org::apache::nifi::minifi::kafka {

class KafkaClientControllerService : public core::controller::ControllerServiceImpl {
public:
  EXTENSIONAPI static constexpr const char* Description = "Manages the client for Apache Kafka. This allows for multiple Kafka related processors "
      "to reference this single client.";

  EXTENSIONAPI static constexpr auto TopicName =
      core::PropertyDefinitionBuilder<>::createProperty("Topic Name")
          .withDescription(
              "The name of the Kafka Topic to pull from.")
          .supportsExpressionLanguage(true)
          .isRequired(true)
          .build();

  EXTENSIONAPI static constexpr auto ConsumerGroup =
    core::PropertyDefinitionBuilder<>::createProperty("Consumer Group")
        .withDescription(
            "Consumer Group")
        .supportsExpressionLanguage(true)
        .isRequired(true)
        .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({TopicName, ConsumerGroup});

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  using ControllerServiceImpl::ControllerServiceImpl;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void yield() override {}
  bool isWorkAvailable() override { return false; }
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }

  void onEnable() override;

  std::shared_ptr<core::FlowFile> poll(core::ProcessSession& session);
  bool commit(core::FlowFile& file);

 private:
  std::string topic_;
  std::string consumer_group_;
  std::unique_ptr<rd_kafka_conf_t, utils::rd_kafka_conf_deleter> conf_ = nullptr;
  std::unique_ptr<rd_kafka_t, utils::rd_kafka_consumer_deleter> consumer_ = nullptr;
  std::unique_ptr<rd_kafka_topic_partition_list_t, utils::rd_kafka_topic_partition_list_deleter> partition_list_ = nullptr;
};
}  // namespace org::apache::nifi::minifi::kafka
