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
#ifndef LIBMINIFI_INCLUDE_UTILS_CPUUTILIZATIONTRACKER_H_
#define LIBMINIFI_INCLUDE_UTILS_CPUUTILIZATIONTRACKER_H_
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace OsUtils {
class CPUUtilizationTracker {
 private:
  uint64_t total_user_;
  uint64_t total_user_low_;
  uint64_t total_sys_;
  uint64_t total_idle_;
  
  uint64_t previous_total_user_;
  uint64_t previous_total_user_low_;
  uint64_t previous_total_sys_;
  uint64_t previous_total_idle_;

  void scanProcStatFile(){
    previous_total_user_ = total_user_;
    previous_total_user_low_ = total_user_low_;
    previous_total_sys_ = total_sys_;
    previous_total_idle_ = total_idle_;
    FILE* file = fopen("/proc/stat", "r");
    fscanf(file, "cpu %llu %llu %llu %llu", &previous_total_user_, &previous_total_user_low_,
        &previous_total_sys_, &previous_total_idle_);
    fclose(file);
  }

  bool currentScanIsOlderThanPrevious() {
    return (total_user_ < previous_total_user_ || 
      total_user_low_ < previous_total_user_low_ ||
      total_sys_ < previous_total_sys_ ||
      total_idle_ < previous_total_idle_);
  }

  double getCurrentValue(){
    double percent;

    if (currentScanIsOlderThanPrevious()){
        //Overflow detection. Just skip this value.
        percent = -1.0;
    } else {
      uint64_t total = (total_user_ - previous_total_user_) + (total_user_low_ - previous_total_user_low_) +
          (total_sys_ - previous_total_sys_);
      percent = total;
      total += (total_idle_ - previous_total_idle_);
      percent /= total;
  }

return percent;
}
};

} /* namespace OsUtils */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_UTILS_CPUUTILIZATIONTRACKER_H_
