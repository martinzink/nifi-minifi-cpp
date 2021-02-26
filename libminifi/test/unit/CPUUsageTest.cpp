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


#include <chrono>
#include <thread>
#include "utils/HostCPULoadTracker.h"
#include "utils/ProcessCPULoadTracker.h"
#include "../TestBase.h"

void busySleep(int duration_ms) {
  auto start = std::chrono::system_clock::now();
  auto now = std::chrono::system_clock::now();
  std::chrono::milliseconds elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds> (now-start);
  while (elapsed_ms < std::chrono::milliseconds(duration_ms)) {
    now = std::chrono::system_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds> (now-start);
  }
}

void idleSleep(int duration_ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
}

TEST_CASE("Test System CPU Utilization Tracker", "[testcpuusage]") {
  org::apache::nifi::minifi::utils::HostCPULoadTracker tracker;
  int vCores = std::thread::hardware_concurrency();

  for (int i = 0; i < 3; ++i) {
    idleSleep(200);
    double systemUtilizationDuringIdleSleep = tracker.getHostCPULoadAndRestartCollection();
    REQUIRE(systemUtilizationDuringIdleSleep >= 0);
    REQUIRE(systemUtilizationDuringIdleSleep <= 1);
    std::cout << "systemUtilizationDuringIdleSleep: " << systemUtilizationDuringIdleSleep << std::endl;

    busySleep(2000);
    double systemUtilizationDuringBusySleep = tracker.getHostCPULoadAndRestartCollection();
    REQUIRE(systemUtilizationDuringBusySleep > (0.8 / vCores));
    REQUIRE(systemUtilizationDuringBusySleep <= 1);
    std::cout << "systemUtilizationDuringBusySleep: " << systemUtilizationDuringBusySleep << std::endl;
  }
}

TEST_CASE("Test Process CPU Utilization Tracker", "[testcpuusage]") {
  org::apache::nifi::minifi::utils::ProcessCPULoadTracker tracker;
  int vCores = std::thread::hardware_concurrency();
  for (int i = 0; i < 3; ++i) {
    idleSleep(200);
    double processCPUUtilizationDuringIdleSleep = tracker.getHostCPULoadAndRestartCollection();
    REQUIRE(processCPUUtilizationDuringIdleSleep < 0.1);
    REQUIRE(processCPUUtilizationDuringIdleSleep >= 0);
    std::cout << "processCPUUtilizationDuringIdleSleep: " << processCPUUtilizationDuringIdleSleep << std::endl;

    busySleep(200);
    double processCPUUtilizationDuringBusySleep = tracker.getHostCPULoadAndRestartCollection();
    REQUIRE(processCPUUtilizationDuringBusySleep > (0.1 / vCores));
    REQUIRE(processCPUUtilizationDuringBusySleep <= 1);
    std::cout << "processCPUUtilizationDuringBusySleep: " << processCPUUtilizationDuringBusySleep << std::endl;
  }
}
