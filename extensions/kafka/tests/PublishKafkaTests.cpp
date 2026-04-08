/**
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
#include "../../../extension-framework/cpp-extension-lib/mocklib/MockLogger.h"
#include "MockProcessContext.h"
#include "MockUtils.h"
#include "PublishKafka.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

namespace org::apache::nifi::minifi::kafka::test {

TEST_CASE("Batch Size cannot be larger than Queue Max Message", "[testPublishKafka]") {
  auto publish_kafka = PublishKafka(mock::getMockMetadata());
  auto context = mock::MockProcessContext{};
  context.properties_.emplace(PublishKafka::ClientName.name, "test_client");
  context.properties_.emplace(PublishKafka::SeedBrokers.name, "test_seedbroker");
  context.properties_.emplace(PublishKafka::QueueBufferMaxMessage.name, "1000");
  context.properties_.emplace(PublishKafka::BatchSize.name, "1500");

  REQUIRE_THROWS_WITH(publish_kafka.onScheduleImpl(context),
      "Process Schedule Operation: Invalid configuration: Batch Size cannot be larger than Queue Max Message");
}

}  // namespace org::apache::nifi::minifi::kafka::test
