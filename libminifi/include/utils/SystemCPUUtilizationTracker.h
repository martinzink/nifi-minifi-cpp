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
#ifndef LIBMINIFI_INCLUDE_UTILS_SYSTEMCPUUTILIZATIONTRACKER_H_
#define LIBMINIFI_INCLUDE_UTILS_SYSTEMCPUUTILIZATIONTRACKER_H_
#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#endif

#ifdef WIN32
#include "TCHAR.h"
#include "pdh.h"
#endif

#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class SystemCPUUtilizationTrackerBase {
 public:
  SystemCPUUtilizationTrackerBase() = default;
  virtual ~SystemCPUUtilizationTrackerBase() = default;
  virtual double getCollectedCPUUtilizationAndRestartCollection() = 0;
};

#ifdef __linux__

class SystemCPUUtilizationTracker : public SystemCPUUtilizationTrackerBase {
 public:
  SystemCPUUtilizationTracker() : total_user_(0), total_user_low_(0), total_sys_(0), total_idle_(0) {
    scanProcStatFile();
  }
  ~SystemCPUUtilizationTracker() = default;

  double getCollectedCPUUtilizationAndRestartCollection() override {
    scanProcStatFile();
    if (isCurrentScanSameAsPrevious() && isCurrentScanOlderThanPrevious()) {
      return -1.0;
    } else {
      return getSystemUtilizationBetweenLastTwoScans();
    }
  }

 protected:
  void scanProcStatFile();

  bool isCurrentScanOlderThanPrevious();

  bool isCurrentScanSameAsPrevious();

  double getSystemUtilizationBetweenLastTwoScans();

 private:
  uint64_t total_user_;
  uint64_t total_user_low_;
  uint64_t total_sys_;
  uint64_t total_idle_;

  uint64_t previous_total_user_;
  uint64_t previous_total_user_low_;
  uint64_t previous_total_sys_;
  uint64_t previous_total_idle_;
};

#endif  // linux

#ifdef WIN32
class SystemCPUUtilizationTracker : public SystemCPUUtilizationTrackerBase {
 public:
  SystemCPUUtilizationTracker() : is_query_open_(false) {
    openQuery();
  }

  ~SystemCPUUtilizationTracker() {
    PdhCloseQuery(cpu_query_);
  }
  double getCollectedCPUUtilizationAndRestartCollection() {
      double value = getValueFromOpenQuery();
      return value;
  }
 protected:
  void openQuery() {
    if (!is_query_open_) {
      if (ERROR_SUCCESS != PdhOpenQuery(NULL, NULL, &cpu_query_))
        return;
      if (ERROR_SUCCESS != PdhAddEnglishCounter(cpu_query_, "\\Processor(_Total)\\% Processor Time", NULL, &cpu_total_)) {
        closeQuery();
        return;
      }
      if (ERROR_SUCCESS != PdhCollectQueryData(cpu_query_)) {
        closeQuery();
        return;
      }
      is_query_open_ = true;
    }
  }

  void closeQuery() {
    PdhCloseQuery(cpu_query_);
  }
  double getValueFromOpenQuery() {
    if (!is_query_open_)
      return -1.0;

    PDH_FMT_COUNTERVALUE counterVal;
    if (ERROR_SUCCESS != PdhCollectQueryData(cpu_query_))
      return -1.0;
    if (ERROR_SUCCESS != PdhGetFormattedCounterValue(cpu_total_, PDH_FMT_DOUBLE, NULL, &counterVal))
      return -1.0;

    return counterVal.doubleValue / 100;
  }

 private:
  PDH_HQUERY cpu_query_;
  PDH_HCOUNTER cpu_total_;
  bool is_query_open_;
};
#endif  // windows

#ifdef __APPLE__
class SystemCPUUtilizationTracker : public SystemCPUUtilizationTrackerBase {
 public:
  SystemCPUUtilizationTracker() : total_ticks_(0), idle_ticks_(0), previous_total_ticks_(0), previous_idle_ticks_(0) {
    scanProcStatFile();
  }
  ~SystemCPUUtilizationTracker() = default;

  double getCollectedCPUUtilizationAndRestartCollection() override {
    queryHostCPULoad();
    if (isCurrentQueryOlderThanPrevious() && isCurrentQuerySameAsPrevious()) {
      return -1.0;
    } else {
      return getSystemUtilizationBetweenLastTwoQueries();
    }
  }

 protected:
  void queryHostCPULoad();

  bool isCurrentQueryOlderThanPrevious();

  bool isCurrentQuerySameAsPrevious();

  double getSystemUtilizationBetweenLastTwoQueries();

 private:
  uint64_t total_ticks_;
  uint64_t idle_ticks_;

  uint64_t previous_total_ticks_;
  uint64_t previous_idle_ticks_;
};
#endif  // macOS

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_UTILS_SYSTEMCPUUTILIZATIONTRACKER_H_
