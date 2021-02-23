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

#include <iostream>
#include <map>

#include "utils/gsl.h"

#ifdef __linux__
#include <sstream>
#include "sys/sysinfo.h"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <sddl.h>
#include <psapi.h>
#include <vector>
#include <algorithm>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#ifdef _WIN32
/*
 These are common translations for SIDs in windows
 */
std::string OsUtils::resolve_common_identifiers(const std::string &id) {
  static std::map<std::string, std::string> nameMap;
  if (nameMap.empty()) {
    nameMap["S-1-0"] = "Null Authority";
    nameMap["S-1-0-0"] = "Nobody";
    nameMap["S-1-1-0"] = "Everyone";
    nameMap["S-1-2"] = "Local Authority";
    nameMap["S-1-2-0"] = "Local";
    nameMap["S-1-2-1"] = "Console Logon";
    nameMap["S-1-3-0"] = "Creator Owner";
    nameMap["S-1-3-1"] = "Creator Group";
  }
  auto name = nameMap.find(id);
  if (name != std::end(nameMap)) {
    return name->second;
  }
  return "";
}
#endif

std::string OsUtils::userIdToUsername(const std::string &uid) {
  std::string name;
  name = uid;
  if (!name.empty()) {
#ifdef _WIN32
    const auto resolved_name = resolve_common_identifiers(name);
    if (!resolved_name.empty()) {
      return resolved_name;
    }
    // First call to LookupAccountSid to get the buffer sizes.
    PSID pSidOwner = NULL;
    const auto guard_pSidOwner = gsl::finally([&pSidOwner]() { if (pSidOwner != NULL) { LocalFree(pSidOwner); } });
    if (ConvertStringSidToSidA(name.c_str(), &pSidOwner)) {
      SID_NAME_USE sidType = SidTypeUnknown;
      DWORD windowsAccountNameSize = 0, dwwindowsDomainSize = 0;
      /*
       We can use a unique ptr with a deleter here but some of the calls
       below require we use global alloc -- so a global deleter to call GlobalFree
       won't buy us a ton unless we anticipate requiring more of this. If we do
       I suggest we break this out until a subset of OsUtils into our own convenience functions.
       */
      LPTSTR windowsDomain = NULL;
      LPTSTR windowsAccount = NULL;

      /*
       The first call will be to obtain sizes for domain and account,
       after which we will allocate the memory and free it after.
       In some cases youc an replace GlobalAlloc with
       */
      LookupAccountSid(NULL /** local computer **/, pSidOwner,
          windowsAccount,
          (LPDWORD)&windowsAccountNameSize,
          windowsDomain,
          (LPDWORD)&dwwindowsDomainSize,
          &sidType);

      if (windowsAccountNameSize > 0) {
        windowsAccount = (LPTSTR)GlobalAlloc(
            GMEM_FIXED,
            windowsAccountNameSize);

        if (dwwindowsDomainSize > 0) {
          windowsDomain = (LPTSTR)GlobalAlloc(
              GMEM_FIXED,
              dwwindowsDomainSize);
        }

        if (LookupAccountSid(
                NULL,
                pSidOwner,
                windowsAccount,
                (LPDWORD)&windowsAccountNameSize,
                windowsDomain,
                (LPDWORD)&dwwindowsDomainSize,
                &sidType)) {
          if (dwwindowsDomainSize > 0) {
            std::string domain = std::string(windowsDomain);
            name = domain + "\\";
            name += std::string(windowsAccount);
          } else {
            name = std::string(windowsAccount);
          }
        }
        GlobalFree(windowsAccount);
        if (dwwindowsDomainSize > 0)
        GlobalFree(windowsDomain);
      }
    }
#else
    auto ptr = name.c_str();
    char *end = nullptr;  // it will be unused
    uid_t ret = std::strtol(ptr, &end, 10);
    if (ret > 0) {
      struct passwd pass;
      struct passwd *result;
      char localbuf[1024];
      if (!getpwuid_r(ret, &pass, localbuf, sizeof localbuf, &result)) {
        name = pass.pw_name;
      }
    }
#endif
  }
  return name;
}

uint64_t OsUtils::getCurrentProcessVirtualMemoryUsage() {
#ifdef __linux__
  static const std::string linePrefix = "VmSize:";
  std::ifstream statusFile("/proc/self/status");
  std::string line;

  while (std::getline(statusFile, line)) {
    // if line begins with "VmSize:"
    if (line.rfind(linePrefix, 0) == 0) {
      std::istringstream valuableLine(line.substr(linePrefix.length()));
      uint64_t kByteValue;
      valuableLine >> kByteValue;
      return kByteValue * 1024;
    }
  }

  throw std::runtime_error("Could not get memory info for current process");
#endif

#ifdef __APPLE__
  task_basic_info tInfo;
  mach_msg_type_number_t tInfoCount = TASK_BASIC_INFO_COUNT;
  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&tInfo, &tInfoCount))
    throw std::runtime_error("Could not get memory info for current process");
  return tInfo.virtual_size;
#endif

#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
    throw std::runtime_error("Could not get memory info for current process");
  return pmc.PrivateUsage;
#endif

  throw std::runtime_error("getCurrentProcessVirtualMemoryUsage() is not implemented for this platform");
}

uint64_t OsUtils::getCurrentProcessPhysicalMemoryUsage() {
#ifdef __linux__
  static const std::string linePrefix = "VmRSS:";
  std::ifstream statusFile("/proc/self/status");
  std::string line;

  while (std::getline(statusFile, line)) {
    // if line begins with "VmRSS:"
    if (line.rfind(linePrefix, 0) == 0) {
      std::istringstream valuableLine(line.substr(linePrefix.length()));
      uint64_t kByteValue;
      valuableLine >> kByteValue;
      return kByteValue * 1024;
    }
  }

  throw std::runtime_error("Could not get memory info for current process");
#endif

#ifdef __APPLE__
  task_basic_info tInfo;
  mach_msg_type_number_t tInfoCount = TASK_BASIC_INFO_COUNT;
  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&tInfo, &tInfoCount))
    throw std::runtime_error("Could not get memory info for current process");
  return tInfo.resident_size;
#endif

#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS pmc;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    throw std::runtime_error("Could not get memory info for current process");
  return pmc.WorkingSetSize;
#endif
  throw std::runtime_error("getCurrentProcessPhysicalMemoryUsage() is not implemented for this platform");
}

uint64_t OsUtils::getSystemVirtualMemoryUsage() {
#ifdef __linux__
  struct sysinfo memInfo;

  sysinfo(&memInfo);
  uint64_t virtualMemUsed = memInfo.totalram - memInfo.freeram;
  virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
  virtualMemUsed *= memInfo.mem_unit;
  return virtualMemUsed;
#endif

#ifdef __APPLE__
  int mib[2];
  struct xsw_usage swap;
  mib[0] = CTL_HW;
  mib[1] = VM_SWAPUSAGE;
  size_t length = sizeof(int64_t);
  sysctl(mib, 2, &swap, &length, NULL, 0);
  return swap.xsu_used + getSystemPhysicalMemoryUsage();
#endif

#ifdef _WIN32
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo)
  DWORDLONG virtualMemUsed = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;
  return virtualMemUsed;
#endif
  throw std::runtime_error("getSystemVirtualMemoryUsage() is not implemented for this platform");
}

uint64_t OsUtils::getSystemPhysicalMemoryUsage() {
#ifdef __linux__
  struct sysinfo memInfo;

  sysinfo(&memInfo);
  uint64_t physMemUsed = memInfo.totalram - memInfo.freeram;
  physMemUsed *= memInfo.mem_unit;
  return physMemUsed;
#endif

#ifdef __APPLE__
  vm_size_t page_size;
  mach_port_t mach_port;
  mach_msg_type_number_t count;
  vm_statistics64_data_t vm_stats;
  mach_port = mach_host_self();
  count = sizeof(vm_stats) / sizeof(natural_t);
  if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
      KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
                                      (host_info64_t)&vm_stats, &count)) {
      uint64_t physMemUsed = ((int64_t)vm_stats.active_count +
                               (int64_t)vm_stats.inactive_count +
                               (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
      return physMemUsed;
  }
  throw std::runtime_error("Could not get memory info");
#endif

#ifdef _WIN32
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
  return physMemUsed;
#endif
  throw std::runtime_error("getSystemPhysicalMemoryUsage() is not implemented for this platform");
}

uint64_t OsUtils::getSystemTotalVirtualMemory() {
#ifdef __linux__
  struct sysinfo memInfo;

  sysinfo(&memInfo);
  uint64_t totalVirtualMem = memInfo.totalram;
  totalVirtualMem += memInfo.totalswap;
  totalVirtualMem *= memInfo.mem_unit;
  return totalVirtualMem;
#endif

#ifdef __APPLE__
  int mib[2];
  struct xsw_usage swap;
  mib[0] = CTL_HW;
  mib[1] = VM_SWAPUSAGE;
  size_t length = sizeof(int64_t);
  sysctl(mib, 2, &swap, &length, NULL, 0);
  return swap.xsu_total + getSystemTotalPhysicalMemory();
#endif

#ifdef _WIN32
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo)
  DWORDLONG totalVirtualMem = memInfo.ullTotalPageFile;
  return totalVirtualMem;
#endif
  throw std::runtime_error("getSystemTotalVirtualMemory() is not implemented for this platform");
}

uint64_t OsUtils::getSystemTotalPhysicalMemory() {
#ifdef __linux__
  struct sysinfo memInfo;

  sysinfo(&memInfo);
  uint64_t totalPhysMem = memInfo.totalram;
  totalPhysMem *= memInfo.mem_unit;
  return totalPhysMem;
#endif

#ifdef __APPLE__
  int mib[2];
  int64_t totalPhysMem;
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  size_t length = sizeof(int64_t);
  sysctl(mib, 2, &totalPhysMem, &length, NULL, 0);
  return totalPhysMem;
#endif

#ifdef _WIN32
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo)
  DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
  return totalPhysMem;
#endif
  throw std::runtime_error("getSystemTotalPhysicalMemory() is not implemented for this platform");
}


}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

