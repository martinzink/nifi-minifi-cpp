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

#include <vector>
#include <unordered_map>
#include <string>

#include "RecordField.h"

namespace org::apache::nifi::minifi::core {


class Record {
 public:
  RecordField& operator[](const std::string& key) {
    return fields_[key];
  }

  const RecordField& operator[](const std::string& key) const {
    return fields_.at(key);
  }

  [[nodiscard]] auto begin() const {
    return fields_.begin();
  }

  [[nodiscard]] auto end() const {
    return fields_.end();
  }

  bool operator==(const Record& rhs) const = default;

 private:
  std::unordered_map<std::string, RecordField> fields_;
};

using RecordSet = std::vector<Record>;

}  // namespace org::apache::nifi::minifi::core