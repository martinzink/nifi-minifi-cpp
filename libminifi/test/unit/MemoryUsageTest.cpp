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


#include "utils/OsUtils.h"
#include "../TestBase.h"

TEST_CASE("Test memory usage", "[testmemoryusage]") {
  std::vector<char> v(30000000);
  const auto vMemUsagebyProcess = utils::OsUtils::getCurrentProcessVirtualMemoryUsage();
  const auto vMemUsagebySystem = utils::OsUtils::getSystemVirtualMemoryUsage();
  const auto vMemTotal = utils::OsUtils::getSystemTotalVirtualMemory();
  const auto RAMUsagebyProcess = utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
  const auto RAMUsagebySystem = utils::OsUtils::getSystemPhysicalMemoryUsage();
  const auto RAMTotal = utils::OsUtils::getSystemTotalPhysicalMemory();
  REQUIRE(vMemUsagebyProcess > v.size());
  REQUIRE(vMemUsagebySystem > vMemUsagebyProcess);
  REQUIRE(vMemTotal >= vMemUsagebySystem);
  REQUIRE(vMemUsagebyProcess >= RAMUsagebyProcess);
  REQUIRE(vMemUsagebySystem > RAMUsagebySystem);
  REQUIRE(vMemTotal >= RAMTotal);
}

#ifndef WIN32
size_t GetTotalMemoryLegacy() {
  return (size_t) sysconf(_SC_PHYS_PAGES) * (size_t) sysconf(_SC_PAGESIZE);
}

TEST_CASE("Test new and legacy total system memory query equivalency") {
  REQUIRE(GetTotalMemoryLegacy() == utils::OsUtils::getSystemTotalPhysicalMemory());
}
#endif
