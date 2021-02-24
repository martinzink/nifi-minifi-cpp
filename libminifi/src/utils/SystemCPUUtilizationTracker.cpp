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

#include "utils/SystemCPUUtilizationTracker.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
#ifdef __linux__
void SystemCPUUtilizationTracker::scanProcStatFile() {
  previous_total_user_ = total_user_;
  previous_total_user_low_ = total_user_low_;
  previous_total_sys_ = total_sys_;
  previous_total_idle_ = total_idle_;
  FILE* file = fopen("/proc/stat", "r");
  fscanf(file, "cpu %lu %lu %lu %lu", &total_user_, &total_user_low_,
         &total_sys_, &total_idle_);
  fclose(file);
}

bool SystemCPUUtilizationTracker::isCurrentScanOlderThanPrevious() {
  return (total_user_ < previous_total_user_ ||
          total_user_low_ < previous_total_user_low_ ||
          total_sys_ < previous_total_sys_ ||
          total_idle_ < previous_total_idle_);
}

bool SystemCPUUtilizationTracker::isCurrentScanSameAsPrevious() {
  return (total_user_ == previous_total_user_ &&
          total_user_low_ == previous_total_user_low_ &&
          total_sys_ == previous_total_sys_ &&
          total_idle_ == previous_total_idle_);
}

double SystemCPUUtilizationTracker::getSystemUtilizationBetweenLastTwoScans() {
  double percent;

  uint64_t total_user_diff = total_user_ - previous_total_user_;
  uint64_t total_user_low_diff = total_user_low_ - previous_total_user_low_;
  uint64_t total_system_diff = total_sys_ - previous_total_sys_;
  uint64_t total_idle_diff = total_idle_ - previous_total_idle_;
  uint64_t total_diff =  total_user_diff + total_user_low_diff + total_system_diff;
  percent = static_cast<double>(total_diff)/static_cast<double>(total_diff+total_idle_diff);

  return percent;
}
#endif  // linux

#ifdef __APPLE__
void SystemCPUUtilizationTracker::queryHostCPULoad() {
  host_cpu_load_info_data_t cpuinfo;
  mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
  if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
    total_ticks_ = 0;
    for (int i = 0; i < CPU_STATE_MAX; i++) {
      totalTicks += cpuinfo.cpu_ticks[i];
    }
    idle_ticks_ = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
  }
}

bool SystemCPUUtilizationTracker::isCurrentQueryOlderThanPrevious() {
  return (total_ticks_ < previous_total_ticks_ ||
          idle_ticks_ < previous_idle_ticks_);
}

bool SystemCPUUtilizationTracker::isCurrentQuerySameAsPrevious() {
  return (total_ticks_ == previous_total_ticks_ &&
          idle_ticks_ == previous_idle_ticks_);
}

double SystemCPUUtilizationTracker::getSystemUtilizationBetweenLastTwoQueries() {
  double percent;

  uint64_t total_ticks_since_last_time = total_ticks_-previous_total_ticks_;
  uint64_t idle_ticks_since_last_time  = idle_ticks_-previous_idle_ticks_;

  percent = static_cast<double>(total_ticks_since_last_time)/static_cast<double>(total_ticks_since_last_time+idle_ticks_since_last_time);

  return percent;
}
#endif  // macOS

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
