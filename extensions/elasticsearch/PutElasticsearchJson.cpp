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


#include "PutElasticsearchJson.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

const core::Relationship PutElasticsearchJson::Success("success", "All flowfiles that succeed in being transferred into Elasticsearch go here. Documents received by the Elasticsearch _bulk API may still result in errors on the Elasticsearch side. The Elasticsearch response will need to be examined to determine whether any Document(s)/Record(s) resulted in errors.");
const core::Relationship PutElasticsearchJson::Failure("failure", "All flowfiles that fail for reasons unrelated to server availability go to this relationship.");
const core::Relationship PutElasticsearchJson::Retry("retry", "All flowfiles that fail due to server/cluster availability go to this relationship.");
const core::Relationship PutElasticsearchJson::Errors("errros", "If a \"Output Error Documents\" is set, any FlowFile(s) corresponding to Elasticsearch document(s) that resulted in an \"error\" (within Elasticsearch) will be routed here.");

const core::Property PutElasticsearchJson::IndexOperation(core::PropertyBuilder::createProperty("Index operation")
    ->withDescription("The type of the operation used to index (create, delete, index, update, upsert)"
                      "Supports Expression Language: true (will be evaluated using flow file attributes and variable registry)")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property PutElasticsearchJson::MaxBatchSize(
    core::PropertyBuilder::createProperty("Max Batch Size")
        ->withDescription("The maximum number of Syslog events to process at a time.")
        ->withDefaultValue<uint64_t>(100)
        ->build());

void PutElasticsearchJson::initialize() {
  setSupportedRelationships({Success, Failure, Retry, Errors});
  setSupportedProperties({IndexOperation, MaxBatchSize});
}

void PutElasticsearchJson::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);

  context->getProperty(MaxBatchSize.getName(), max_batch_size_);
}

namespace {
class BulkOperation {
 private:

  PutElasticsearchJson::IndexOperations operation;
};
}

void PutElasticsearchJson::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && max_batch_size_ > 0);
  size_t logs_processed = 0;
  std::vector<std::string> operations;
  while (operations.size() < max_batch_size_) {
    auto flow_file = session->get();
    if (!flow_file)
      break;
    auto index_operation_str = context->getProperty(IndexOperation, flow_file);
    if (!index_operation_str) {
      session->transfer(flow_file, Failure);
    }
  try {
    auto index_operation = IndexOperations::parse(index_operation_str->c_str());
  } catch (std::runtime_error) {

  }
  }
}

}  // org::apache::nifi::minifi::extensions::elasticsearch
