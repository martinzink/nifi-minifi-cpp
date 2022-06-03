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
  auto elasticsearch_client_controller_service = test_controller.plan->addController("ElasticsearchClientControllerService", "elasticsearch_client_controller_service");
  test_controller.plan->setProperty(put_elasticsearch_json,
                                     PutElasticsearchJson::ClientService.getName(),
                                     "elastic_search_client_controller_service");

}

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
