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

#include <string>
#include <memory>

#include "controllers/SSLContextService.h"
#include "core/Processor.h"
#include "utils/Enum.h"
#include "client/HTTPClient.h"
#include "utils/HTTPClient.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

class PutElasticsearchJson : public core::Processor {
 public:
  explicit PutElasticsearchJson(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
  }
  ~PutElasticsearchJson() override = default;

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  EXTENSIONAPI static const core::Relationship Retry;
  EXTENSIONAPI static const core::Relationship Errors;

  EXTENSIONAPI static const core::Property IndexOperation;
  EXTENSIONAPI static const core::Property MaxBatchSize;
  EXTENSIONAPI static const core::Property ElasticCredentials;
  EXTENSIONAPI static const core::Property SSLContext;
  EXTENSIONAPI static const core::Property Hosts;
  EXTENSIONAPI static const core::Property Index;
  EXTENSIONAPI static const core::Property Id;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

 private:
  uint64_t max_batch_size_;
  utils::HTTPClient client_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutElasticsearchJson>::getLogger();
};

}  // org::apache::nifi::minifi::extensions::elasticsearch
