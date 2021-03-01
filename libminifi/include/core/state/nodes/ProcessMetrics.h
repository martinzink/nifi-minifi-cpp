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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_PROCESSMETRICS_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_PROCESSMETRICS_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/Resource.h"

#ifndef WIN32
#include <sys/resource.h>

#endif

#include "../nodes/DeviceInformation.h"
#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "../../../utils/OsUtils.h"
#include "../../../utils/ProcessCPULoadTracker.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Justification and Purpose: Provides process metrics. Provides critical information to the
 * C2 server.
 *
 */
class ProcessMetrics : public ResponseNode {
 public:
  ProcessMetrics(const std::string &name, const utils::Identifier &uuid)
      : ResponseNode(name, uuid) {
  }

  ProcessMetrics(const std::string &name) // NOLINT
      : ResponseNode(name) {
  }

  ProcessMetrics() = default;

  virtual std::string getName() const {
    return "ProcessMetrics";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

#ifndef WIN32
    struct rusage my_usage;
    getrusage(RUSAGE_SELF, &my_usage);
#endif
    SerializedResponseNode memory;
    memory.name = "MemoryMetrics";
    memory.children.push_back(serializePhysicalMemoryUsageInformation());
#ifndef WIN32
    memory.children.push_back(serializeMaximumResidentSetSize(my_usage));
#endif
    serialized.push_back(memory);

    SerializedResponseNode cpu;
    cpu.name = "CpuMetrics";
    cpu.children.push_back(serializeProcessCPUUsageInformation());
#ifndef WIN32
    cpu.children.push_back(serializeInvoluntaryContextSwitches(my_usage));
#endif
    serialized.push_back(cpu);
    return serialized;
  }

 protected:
#ifndef WIN32
  SerializedResponseNode serializeInvoluntaryContextSwitches(const struct rusage& my_usage) {
    SerializedResponseNode ics;
    ics.name = "involcs";
    ics.value = (uint64_t)my_usage.ru_nivcsw;

    return ics;
  }

  SerializedResponseNode serializeMaximumResidentSetSize(const struct rusage& my_usage) {
    SerializedResponseNode maxrss;
    maxrss.name = "maxrss";
    maxrss.value = (uint64_t)my_usage.ru_maxrss;

    return maxrss;
  }
#endif

  SerializedResponseNode serializePhysicalMemoryUsageInformation() {
    SerializedResponseNode used_physical_memory;
    used_physical_memory.name = "memoryUtilization";
    used_physical_memory.value = (uint64_t)utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
    return used_physical_memory;
  }

  SerializedResponseNode serializeProcessCPUUsageInformation() {
    double system_cpu_usage = -1.0;
    {
      std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
      system_cpu_usage = cpu_load_tracker_.getProcessCPULoadAndRestartCollection();
    }
    SerializedResponseNode cpu_usage;
    cpu_usage.name = "cpuUtilization";
    cpu_usage.value = system_cpu_usage;
    return cpu_usage;
  }

 private:
  static utils::ProcessCPULoadTracker cpu_load_tracker_;
  static std::mutex cpu_load_tracker_mutex_;
};

REGISTER_RESOURCE(ProcessMetrics, "Node part of an AST that defines the Processor information and metrics subtree");

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_PROCESSMETRICS_H_
