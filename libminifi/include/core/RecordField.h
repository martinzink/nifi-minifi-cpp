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

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace org::apache::nifi::minifi::core {

struct RecordField;

using RecordArray = std::vector<RecordField>;
using RecordObject = std::unordered_map<std::string, std::unique_ptr<RecordField>>;

struct RecordField {
  std::variant<std::string, int64_t, double, bool, std::chrono::system_clock::time_point, RecordArray, RecordObject> value_;

  bool operator==(const RecordField& rhs) const = default;
};

}  // namespace org::apache::nifi::minifi::core
