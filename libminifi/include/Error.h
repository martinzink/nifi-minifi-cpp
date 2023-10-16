/**
 * @file Exception.h
 * Exception class declaration
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

#include <system_error>

namespace org::apache::nifi::minifi {

enum class Errc {
  // no 0
  FileOperationErr = 1,
  FlowErr,
  ProcessorErr,
  ProcessSessionErr,
  ProcessScheduleErr,
  Site2SiteErr,
  GeneralErr,
  RegexErr,
  RepositoryErr,
  StreamError
};

std::error_code make_error_code(Errc);

std::error_code getLastOsError();

}  // namespace org::apache::nifi::minifi

namespace std {
template <>
struct is_error_code_enum<org::apache::nifi::minifi::Errc> : true_type {};
}  // namespace std

