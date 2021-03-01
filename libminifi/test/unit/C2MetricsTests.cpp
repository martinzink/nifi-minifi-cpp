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
#include <memory>

#include "../../include/core/state/nodes/ProcessMetrics.h"
#include "../../include/core/state/nodes/QueueMetrics.h"
#include "../../include/core/state/nodes/RepositoryMetrics.h"
#include "../../include/core/state/nodes/SystemMetrics.h"
#include "../TestBase.h"
#include "core/ClassLoader.h"

TEST_CASE("TestProcessMetrics", "[c2m1]") {
  minifi::state::response::ProcessMetrics metrics;

  REQUIRE("ProcessMetrics" == metrics.getName());
  auto serialized_metrics = metrics.serialize();
  REQUIRE(2 == serialized_metrics.size());

  {
    auto memory_metrics = serialized_metrics.at(0);
    REQUIRE("MemoryMetrics" == memory_metrics.name);
    auto used_memory = memory_metrics.children.at(0);
    REQUIRE("memoryUtilization" == used_memory.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::UINT64_TYPE == used_memory.value.getValue()->getTypeIndex());
#ifndef WIN32
    auto max_resident_set_size = memory_metrics.children.at(1);
    REQUIRE("maxrss" == max_resident_set_size.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::UINT64_TYPE == max_resident_set_size.value.getValue()->getTypeIndex());
#endif
  }
  {
    auto cpu_metrics = serialized_metrics.at(1);
    REQUIRE("CpuMetrics" == cpu_metrics.name);
    auto cpu_usage = cpu_metrics.children.at(0);
    REQUIRE("cpuUtilization" == cpu_usage.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::DOUBLE_TYPE == cpu_usage.value.getValue()->getTypeIndex());
#ifndef WIN32
    auto involuntary_context_switches = cpu_metrics.children.at(1);
    REQUIRE("involcs" == involuntary_context_switches.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::UINT64_TYPE == involuntary_context_switches.value.getValue()->getTypeIndex());
#endif
  }
}

TEST_CASE("TestSystemMetrics", "[c2m5]") {
  minifi::state::response::SystemInformation metrics;

  REQUIRE("systeminfo" == metrics.getName());

  auto serialized_metrics = metrics.serialize();

  REQUIRE(2 == serialized_metrics.size());

  auto system_info = serialized_metrics.at(0);
  REQUIRE("systemInfo" == system_info.name);

  {
    auto vcores_info = system_info.children.at(0);
    REQUIRE("vCores" == vcores_info.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::UINT32_TYPE == vcores_info.value.getValue()->getTypeIndex());
  }
  {
    auto ram_info = system_info.children.at(1);
    REQUIRE("physicalMem" == ram_info.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::UINT64_TYPE == ram_info.value.getValue()->getTypeIndex());
  }
  {
    auto used_ram_info = system_info.children.at(2);
    REQUIRE("memoryUtilization" == used_ram_info.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::UINT64_TYPE == used_ram_info.value.getValue()->getTypeIndex());
  }
  {
    auto cpu_usage_info = system_info.children.at(3);
    REQUIRE("cpuUtilization" == cpu_usage_info.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::DOUBLE_TYPE == cpu_usage_info.value.getValue()->getTypeIndex());
  }
  {
    auto system_architecture = system_info.children.at(4);
    REQUIRE("machinearch" == system_architecture.name);
    REQUIRE(org::apache::nifi::minifi::state::response::Value::STRING_TYPE == system_architecture.value.getValue()->getTypeIndex());
  }
  auto identifier = serialized_metrics.at(1);
  REQUIRE("identifier" == identifier.name);
}

TEST_CASE("QueueMetricsTestNoConnections", "[c2m2]") {
  minifi::state::response::QueueMetrics metrics;

  REQUIRE("QueueMetrics" == metrics.getName());

  REQUIRE(0 == metrics.serialize().size());
}

TEST_CASE("QueueMetricsTestConnections", "[c2m3]") {
  minifi::state::response::QueueMetrics metrics;

  REQUIRE("QueueMetrics" == metrics.getName());

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  content_repo->initialize(configuration);

  std::shared_ptr<core::Repository> repo = std::make_shared<TestRepository>();

  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(repo, content_repo, "testconnection");

  metrics.addConnection(connection);

  connection->setMaxQueueDataSize(1024);
  connection->setMaxQueueSize(1024);

  REQUIRE(1 == metrics.serialize().size());

  minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

  REQUIRE("testconnection" == resp.name);

  REQUIRE(4 == resp.children.size());

  minifi::state::response::SerializedResponseNode datasize = resp.children.at(0);

  REQUIRE("datasize" == datasize.name);
  REQUIRE("0" == datasize.value.to_string());

  minifi::state::response::SerializedResponseNode datasizemax = resp.children.at(1);

  REQUIRE("datasizemax" == datasizemax.name);
  REQUIRE("1024" == datasizemax.value);

  minifi::state::response::SerializedResponseNode queued = resp.children.at(2);

  REQUIRE("queued" == queued.name);
  REQUIRE("0" == queued.value.to_string());

  minifi::state::response::SerializedResponseNode queuedmax = resp.children.at(3);

  REQUIRE("queuedmax" == queuedmax.name);
  REQUIRE("1024" == queuedmax.value.to_string());
}

TEST_CASE("RepositorymetricsNoRepo", "[c2m4]") {
  minifi::state::response::RepositoryMetrics metrics;

  REQUIRE("RepositoryMetrics" == metrics.getName());

  REQUIRE(0 == metrics.serialize().size());
}

TEST_CASE("RepositorymetricsHaveRepo", "[c2m4]") {
  minifi::state::response::RepositoryMetrics metrics;

  REQUIRE("RepositoryMetrics" == metrics.getName());

  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  metrics.addRepository(repo);
  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::response::SerializedResponseNode running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("false" == running.value.to_string());

    minifi::state::response::SerializedResponseNode full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("false" == full.value);

    minifi::state::response::SerializedResponseNode size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value);
  }

  repo->start();
  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::response::SerializedResponseNode running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("true" == running.value.to_string());

    minifi::state::response::SerializedResponseNode full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("false" == full.value);

    minifi::state::response::SerializedResponseNode size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value);
  }

  repo->setFull();

  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::response::SerializedResponseNode running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("true" == running.value.to_string());

    minifi::state::response::SerializedResponseNode full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("true" == full.value.to_string());

    minifi::state::response::SerializedResponseNode size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value.to_string());
  }

  repo->stop();

  {
    REQUIRE(1 == metrics.serialize().size());

    minifi::state::response::SerializedResponseNode resp = metrics.serialize().at(0);

    REQUIRE("repo_name" == resp.name);

    REQUIRE(3 == resp.children.size());

    minifi::state::response::SerializedResponseNode running = resp.children.at(0);

    REQUIRE("running" == running.name);
    REQUIRE("false" == running.value.to_string());

    minifi::state::response::SerializedResponseNode full = resp.children.at(1);

    REQUIRE("full" == full.name);
    REQUIRE("true" == full.value);

    minifi::state::response::SerializedResponseNode size = resp.children.at(2);

    REQUIRE("size" == size.name);
    REQUIRE("0" == size.value);
  }
}
