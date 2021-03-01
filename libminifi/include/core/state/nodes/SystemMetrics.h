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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_SYSTEMMETRICS_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_SYSTEMMETRICS_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/Resource.h"

#include "../nodes/DeviceInformation.h"
#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "../../../utils/OsUtils.h"
#include "../../../utils/HostCPULoadTracker.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Justification and Purpose: Provides system information, including critical device information.
 *
 */
class SystemInformation : public DeviceInformation {
 public:
  SystemInformation(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
  }

  SystemInformation(const std::string &name) // NOLINT
      : DeviceInformation(name) {
  }

  SystemInformation()
      : DeviceInformation("systeminfo") {
  }

  virtual std::string getName() const {
    return "systeminfo";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;
    serialized.push_back(serializeSystemInformation());
    serialized.push_back(serializeIdentifier());
    return serialized;
  }

 protected:
  SerializedResponseNode serializeSystemInformation() {
    SerializedResponseNode systemInfo;
    systemInfo.name = "systemInfo";

    systemInfo.children.push_back(serializeVCoreInformation());
    systemInfo.children.push_back(serializeTotalPhysicalMemoryInformation());
    systemInfo.children.push_back(serializePhysicalMemoryUsageInformation());
    systemInfo.children.push_back(serializeSystemCPUUsageInformation());
    systemInfo.children.push_back(serializeArchitectureInformation());

    return systemInfo;
  }

  SerializedResponseNode serializeIdentifier() {
    SerializedResponseNode identifier;
    identifier.name = "identifier";
    identifier.value = "identifier";
    return identifier;
  }

  SerializedResponseNode serializeVCoreInformation() {
    SerializedResponseNode vcores;
    vcores.name = "vCores";
    size_t ncpus = std::thread::hardware_concurrency();

    vcores.value = (uint32_t)ncpus;
    return vcores;
  }

  SerializedResponseNode serializeTotalPhysicalMemoryInformation() {
    SerializedResponseNode total_physical_memory;
    total_physical_memory.name = "physicalMem";
    total_physical_memory.value = (uint64_t)utils::OsUtils::getSystemTotalPhysicalMemory();
    return total_physical_memory;
  }

  SerializedResponseNode serializePhysicalMemoryUsageInformation() {
    SerializedResponseNode used_physical_memory;
    used_physical_memory.name = "memoryUtilization";
    used_physical_memory.value = (uint64_t)utils::OsUtils::getSystemPhysicalMemoryUsage();
    return used_physical_memory;
  }

  SerializedResponseNode serializeSystemCPUUsageInformation() {
    double system_cpu_usage = -1.0;
    {
      std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
      system_cpu_usage = cpu_load_tracker_.getHostCPULoadAndRestartCollection();
    }
    SerializedResponseNode cpu_usage;
    cpu_usage.name = "cpuUtilization";
    cpu_usage.value = system_cpu_usage;
    return cpu_usage;
  }

  SerializedResponseNode serializeArchitectureInformation() {
    SerializedResponseNode arch;
    arch.name = "machinearch";
    arch.value = utils::OsUtils::getSystemArchitecture();
    return arch;
  }

 private:
  static utils::HostCPULoadTracker cpu_load_tracker_;
  static std::mutex cpu_load_tracker_mutex_;
};

REGISTER_RESOURCE(SystemInformation, "Node part of an AST that defines the System information and metrics subtree");
}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_SYSTEMMETRICS_H_
