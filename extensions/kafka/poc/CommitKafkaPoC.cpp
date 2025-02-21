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

#include "CommitKafkaPoC.h"
#include "core/FlowFile.h"
#include "core/ProcessSession.h"
#include "core/PropertyType.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::kafka {

void CommitKafkaPoC::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  kafka_client_controller_service_ = minifi::utils::parseControllerService<KafkaClientControllerService>(context, KafkaClient, getUUID());
}

void CommitKafkaPoC::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(kafka_client_controller_service_);
  std::vector<std::shared_ptr<core::FlowFile>> flow_files;
  while (auto ff = session.get()) {
    flow_files.push_back(std::move(ff));
  }
  if (flow_files.empty()) {
    context.yield();
    return;
  }
  gsl_Assert(kafka_client_controller_service_->commit(*flow_files.back()));
  for (auto ff : flow_files) {
    session.transfer(ff, Success);
  }
}

void CommitKafkaPoC::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

REGISTER_RESOURCE(CommitKafkaPoC, Processor);
}  // namespace org::apache::nifi::minifi::kafka
