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

#include "utils/ProcessCPUUtilizationTracker.h"
#ifndef WIN32
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/times.h>
#endif
#include <thread>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
#if defined(__linux__) || defined(__APPLE__)
void ProcessCPUUtilizationTracker::queryCPUTimes() {
  previous_cpu_times_ = cpu_times_;
  previous_sys_cpu_times_ = sys_cpu_times_;
  previous_user_cpu_times_ = user_cpu_times_;
  struct tms timeSample;
  cpu_times_ = times(&timeSample);
  sys_cpu_times_ = timeSample.tms_stime;
  user_cpu_times_ = timeSample.tms_utime;
}

bool ProcessCPUUtilizationTracker::isCurrentScanOlderThanPrevious() {
  return (cpu_times_ < previous_cpu_times_ ||
      sys_cpu_times_ < previous_sys_cpu_times_ ||
      user_cpu_times_ < previous_user_cpu_times_);
}

bool ProcessCPUUtilizationTracker::isCurrentScanSameAsPrevious() {
  return (cpu_times_ == previous_cpu_times_ &&
      sys_cpu_times_ == previous_sys_cpu_times_ &&
      user_cpu_times_ == previous_user_cpu_times_);
}

double ProcessCPUUtilizationTracker::getProcessUtilizationSinceLastScan() {
  double percent;

  if (isCurrentScanOlderThanPrevious() || isCurrentScanSameAsPrevious()) {
    percent = -1.0;
  } else {
    clock_t cpu_times_diff = cpu_times_ - previous_cpu_times_;
    clock_t sys_cpu_times_diff = sys_cpu_times_ - previous_sys_cpu_times_;
    clock_t user_cpu_times_diff = user_cpu_times_ - previous_user_cpu_times_;
    percent = static_cast<double>(sys_cpu_times_diff + user_cpu_times_diff)/static_cast<double>(cpu_times_diff);
    percent = percent / std::thread::hardware_concurrency();
  }
  return percent;
}
#endif

#if defined(WIN32)
bool ProcessCPUUtilizationTracker::isCurrentScanOlderThanPrevious() {
    return (cpu_times_ < previous_cpu_times_ ||
        sys_cpu_times_ < previous_sys_cpu_times_ ||
        user_cpu_times_ < previous_user_cpu_times_);
}

bool ProcessCPUUtilizationTracker::isCurrentScanSameAsPrevious() {
    return (cpu_times_ == previous_cpu_times_ &&
        sys_cpu_times_ == previous_sys_cpu_times_ &&
        user_cpu_times_ == previous_user_cpu_times_);
}

void ProcessCPUUtilizationTracker::queryCPUTimes() {
    previous_cpu_times_ = cpu_times_;
    previous_sys_cpu_times_ = sys_cpu_times_;
    previous_user_cpu_times_ = user_cpu_times_;
    SYSTEM_INFO sysInfo;
    FILETIME ftime, fsys, fuser;
    GetSystemTimeAsFileTime(&ftime);

    cpu_times_ = ULARGE_INTEGER{ ftime.dwLowDateTime, ftime.dwHighDateTime }.QuadPart;

    GetProcessTimes(self_, &ftime, &ftime, &fsys, &fuser);
    sys_cpu_times_ = ULARGE_INTEGER{ fsys.dwLowDateTime, fsys.dwHighDateTime }.QuadPart;
    user_cpu_times_ = ULARGE_INTEGER{ fuser.dwLowDateTime, fuser.dwHighDateTime }.QuadPart;
}

double ProcessCPUUtilizationTracker::getProcessUtilizationSinceLastScan() {
    double percent;

    if (isCurrentScanOlderThanPrevious() || isCurrentScanSameAsPrevious()) {
        percent = -1.0;
    } else {
        uint64_t cpu_times_diff = cpu_times_ - previous_cpu_times_;
        uint64_t sys_cpu_times_diff = sys_cpu_times_ - previous_sys_cpu_times_;
        uint64_t user_cpu_times_diff = user_cpu_times_ - previous_user_cpu_times_;
        percent = static_cast<double>(sys_cpu_times_diff + user_cpu_times_diff) / static_cast<double>(cpu_times_diff);
        percent = percent / std::thread::hardware_concurrency();
    }
    return percent;
}
#endif

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
