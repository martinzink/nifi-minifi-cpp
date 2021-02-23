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
#include "utils/SystemCPUUtilizationTracker.h"
#include "utils/ProcessCPUUtilizationTracker.h"
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
  org::apache::nifi::minifi::utils::SystemCPUUtilizationTracker tracker;
  int vCores = std::thread::hardware_concurrency();

  idleSleep(100);
  tracker.scanProcStatFile();
  double systemUtilizationDuringIdleSleep = tracker.getSystemUtilizationSinceLastScan();
  REQUIRE(!tracker.isCurrentScanSameAsPrevious());
  REQUIRE(systemUtilizationDuringIdleSleep > 0);

  busySleep(100);
  tracker.scanProcStatFile();
  double systemUtilizationDuringBusySleep = tracker.getSystemUtilizationSinceLastScan();
  REQUIRE(!tracker.isCurrentScanSameAsPrevious());
  REQUIRE(systemUtilizationDuringBusySleep > (0.9/vCores));
}

TEST_CASE("Test Process CPU Utilization Tracker", "[testcpuusage]") {
  org::apache::nifi::minifi::utils::ProcessCPUUtilizationTracker tracker;
  int vCores = std::thread::hardware_concurrency();

  idleSleep(100);
  tracker.queryCPUTimes();
  double processCPUUtilizationDuringIdleSleep = tracker.getProcessUtilizationSinceLastScan();
  REQUIRE(!tracker.isCurrentScanSameAsPrevious());
  REQUIRE(processCPUUtilizationDuringIdleSleep < 0.1);

  busySleep(100);
  tracker.queryCPUTimes();
  double processCPUUtilizationDuringBusySleep = tracker.getProcessUtilizationSinceLastScan();
  REQUIRE(!tracker.isCurrentScanSameAsPrevious());
  REQUIRE(processCPUUtilizationDuringBusySleep > (0.9/vCores));
}
