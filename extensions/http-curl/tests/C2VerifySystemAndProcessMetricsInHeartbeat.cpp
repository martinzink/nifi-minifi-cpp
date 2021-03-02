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

#undef NDEBUG
#include "TestBase.h"
#include "c2/C2Agent.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "protocols/RESTReceiver.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"

class SystemAndProcessMetricsInHeartbeatHandler : public HeartbeatHandler {
 public:
  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection *) override {
    //verifySystemMetrics(root, (calls_ == 0));
    verifyProcessMetrics(root, (calls_ == 0));
    ++calls_;
  }

  size_t getNumberOfHandledHeartBeats() {
    return calls_;
  }

 protected:
  void verifySystemMetrics(const rapidjson::Document& root, bool firstCall) {
    assert(root.HasMember("systeminfo"));
    auto& system_info = root["systeminfo"];

    assert(system_info.HasMember("vCores"));
    assert(system_info["vCores"].GetUint() > 0);

    assert(system_info.HasMember("physicalMem"));
    assert(system_info["physicalMem"].GetUint64() > 0);

    assert(system_info.HasMember("memoryUtilization"));
    assert(system_info["memoryUtilization"].GetUint64() > 0);

    assert(system_info.HasMember("cpuUtilization"));
    if (!firstCall) {
      assert(system_info["cpuUtilization"].GetDouble() >= 0.0);
      assert(system_info["cpuUtilization"].GetDouble() <= 1.0);
    }

    assert(system_info.HasMember("machineArch"));
    assert(system_info["machineArch"].GetStringLength() > 0);
  }

  void verifyProcessMetrics(const rapidjson::Document& root, bool firstCall) {
    assert(root.HasMember("ProcessMetrics"));
    auto& process_metrics = root["ProcessMetrics"];

    assert(process_metrics.HasMember("MemoryMetrics"));
    auto& memory_metrics = process_metrics["MemoryMetrics"];

    assert(memory_metrics.HasMember("memoryUtilization"));
    assert(memory_metrics["memoryUtilization"].GetUint64() > 0);

    assert(process_metrics.HasMember("CpuMetrics"));
    auto& cpu_metrics = process_metrics["CpuMetrics"];

    assert(cpu_metrics.HasMember("cpuUtilization"));
    auto& cpu_utilization = cpu_metrics["cpuUtilization"];
    assert(!cpu_utilization.IsString());
    assert(cpu_utilization.IsDouble());
    if (!firstCall) {
      assert(cpu_utilization.GetDouble() >= 0.0);
      assert(cpu_utilization.GetDouble() <= 1.0);
    }
  }

 private:
  std::atomic<size_t> calls_{0};
};

class VerifySystemAndProcessMetricsInHeartbeat : public VerifyC2Base {
 public:

  VerifySystemAndProcessMetricsInHeartbeat(std::function<bool()> event_to_wait_for) :
      event_to_wait_for_(event_to_wait_for), VerifyC2Base() {
  }

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,SystemInformation,ProcessMetrics,AgentInformation,FlowInformation");
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTReceiver>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::milliseconds(600000),event_to_wait_for_));
  }

  void configureFullHeartbeat() override {
    configuration->set("nifi.c2.full.heartbeat", "false");
  }

  std::function<bool()> event_to_wait_for_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");

  SystemAndProcessMetricsInHeartbeatHandler responder;
  auto event_to_wait_for = [&responder] {
    return responder.getNumberOfHandledHeartBeats() >= 3;
  };
  VerifySystemAndProcessMetricsInHeartbeat harness(event_to_wait_for);
  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);

  return 0;
}
