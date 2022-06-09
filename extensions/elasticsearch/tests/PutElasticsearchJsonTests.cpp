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

#include "../PutElasticsearchJson.h"
#include "../ElasticsearchCredentialsControllerService.h"
#include "MockElastic.h"
#include "SingleProcessorTestController.h"
#include "Catch.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch::test {

TEST_CASE("PutElasticsearchJson", "[elastic]") {
  MockElastic mock_elastic("10433");
  mock_elastic.setAssertions([](const struct mg_request_info *request_info) {
    CHECK(request_info->query_string == nullptr);
  });

  std::shared_ptr<PutElasticsearchJson> put_elasticsearch_json = std::make_shared<PutElasticsearchJson>("PutElasticsearchJson");
  minifi::test::SingleProcessorTestController test_controller{put_elasticsearch_json};
  auto elasticsearch_credentials_controller_service = test_controller.plan->addController("ElasticsearchCredentialsControllerService", "elasticsearch_credentials_controller_service");
  test_controller.plan->setProperty(put_elasticsearch_json,
                                     PutElasticsearchJson::ElasticCredentials.getName(),
                                     "elasticsearch_credentials_controller_service");
  test_controller.plan->setProperty(put_elasticsearch_json,
                                    PutElasticsearchJson::Hosts.getName(),
                                    "localhost:10433");
  test_controller.plan->setProperty(put_elasticsearch_json,
                                    PutElasticsearchJson::IndexOperation.getName(),
                                    "index");
  test_controller.plan->setProperty(put_elasticsearch_json,
                                    PutElasticsearchJson::Index.getName(),
                                    "test_index");

  test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                    ElasticsearchCredentialsControllerService::CredentialsType.getName(),
                                    toString(ElasticsearchCredentialsControllerService::CredType::USE_API_KEY));

  test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                    ElasticsearchCredentialsControllerService::ApiKey.getName(),
                                    MockElastic::API_KEY);

  auto results = test_controller.trigger(R"({"field1":"value1"}")");
  CHECK(results[PutElasticsearchJson::Success].size() == 1);
}

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
