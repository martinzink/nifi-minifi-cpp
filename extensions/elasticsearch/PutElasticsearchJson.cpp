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
#include "ElasticsearchCredentialsControllerService.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "rapidjson/document.h"

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

const core::Property PutElasticsearchJson::ElasticCredentials(
    core::PropertyBuilder::createProperty("Elasticsearch Credentials Provider Service")
        ->withDescription("The Controller Service used to obtain Elasticsearch credentials.")
        ->isRequired(true)
        ->asType<ElasticsearchCredentialsControllerService>()
        ->build());

const core::Property PutElasticsearchJson::SSLContext(
    core::PropertyBuilder::createProperty("SSL Context Service")
        ->withDescription("The SSL Context Service used to provide client certificate "
                          "information for TLS/SSL (https) connections.")
        ->isRequired(false)
        ->asType<minifi::controllers::SSLContextService>()->build());

const core::Property PutElasticsearchJson::Hosts(core::PropertyBuilder::createProperty("Hosts")
    ->withDescription("A comma-separated list of HTTP hosts that host Elasticsearch query nodes. Currently only supports a single host.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property PutElasticsearchJson::Index(core::PropertyBuilder::createProperty("Hosts")
    ->withDescription("The name of the index to use.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property PutElasticsearchJson::Id(core::PropertyBuilder::createProperty("Id")
    ->withDescription("If the Index Operation is \"index\", this property may be left empty or evaluate to an empty value, "
                      "in which case the document's identifier will be auto-generated by Elasticsearch. "
                      "For all other Index Operations, the attribute must evaluate to a non-empty value.")
    ->supportsExpressionLanguage(true)
    ->build());


void PutElasticsearchJson::initialize() {
  setSupportedRelationships({Success, Failure, Retry, Errors});
  setSupportedProperties({ElasticCredentials, IndexOperation, MaxBatchSize, Hosts, SSLContext});
}

namespace {
auto getSSLContextService(core::ProcessContext& context) {
  if (auto ssl_context = context.getProperty(PutElasticsearchJson::SSLContext))
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(context.getControllerService(*ssl_context));
  return std::shared_ptr<minifi::controllers::SSLContextService>{};
}

auto getCredentialsService(core::ProcessContext& context) {
  if (auto credentials = context.getProperty(PutElasticsearchJson::ElasticCredentials))
    return std::dynamic_pointer_cast<ElasticsearchCredentialsControllerService>(context.getControllerService(*credentials));
  return std::shared_ptr<ElasticsearchCredentialsControllerService>{};
}
}  // namespace

void PutElasticsearchJson::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);

  context->getProperty(MaxBatchSize.getName(), max_batch_size_);
  if (max_batch_size_ < 1)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Max Batch Size property is invalid");

  std::string host_url;
  if (auto hosts_str = context->getProperty(Hosts)) {
    auto hosts = utils::StringUtils::split(*hosts_str, ",");
    if (hosts.size() != 1)
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Multiple hosts not yet supported");
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing or invalid hosts");
  }

  auto credentials_service = getCredentialsService(*context);
  if (!credentials_service)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Missing Elasticsearch credentials service");

  client_.initialize("POST", host_url, getSSLContextService(*context));
  credentials_service->authenticateClient(client_);
}

namespace {
class OperationRequest {
 public:
  SMART_ENUM(OpType,
             (INDEX, "index"),
             (CREATE, "create"),
             (DELETE, "delete"),
             (UPDATE, "update"))

  OperationRequest(OpType type, rapidjson::Value)
      : type_(type) {
  }

  std::string toString() const {
    return "";
  }
 private:
  OpType type_;
  std::string index_;
  std::optional<std::string> id_;
  std::optional<std::unordered_map<std::string, std::string>> fields_;
};

std::optional<OperationRequest> parseOperationRequest(core::ProcessContext&, core::FlowFile&) {
  return std::nullopt;
}

std::string buildRequest(std::vector<OperationRequest> requests) {

  return "";
}
}

void PutElasticsearchJson::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && max_batch_size_ > 0);
  std::vector<OperationRequest> requests;
  while (requests.size() < max_batch_size_) {
    auto flow_file = session->get();
    if (!flow_file)
      break;
    auto operation_request = parseOperationRequest(*context, *flow_file);
    if (!operation_request)
      session->transfer(flow_file, PutElasticsearchJson::Failure);
  }

}

}  // org::apache::nifi::minifi::extensions::elasticsearch
