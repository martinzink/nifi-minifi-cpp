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

#pragma once

#include <cstdint>

constexpr uint64_t operator""_KiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024 * n;
}

constexpr uint64_t operator""_MiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_KiB * n;
}

constexpr uint64_t operator""_GiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_MiB * n;
}

constexpr uint64_t operator""_TiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_GiB * n;
}

constexpr uint64_t operator""_PiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_TiB * n;
}

constexpr uint64_t operator""_KB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000 * n;
}

constexpr uint64_t operator""_MB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_KB * n;
}

constexpr uint64_t operator""_GB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_MB * n;
}

constexpr uint64_t operator""_TB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_GB * n;
}

constexpr uint64_t operator""_PB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_TB * n;
}
