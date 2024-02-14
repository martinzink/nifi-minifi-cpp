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

#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::core {

struct RecordField;

using RecordArray = std::vector<RecordField>;
using RecordObject = std::unordered_map<std::string, RecordField>;

struct RecordField {
  std::variant<std::string, int64_t, double, bool, std::chrono::system_clock::time_point, RecordArray, RecordObject> value_;
};

template<typename RecordFieldType>
rapidjson::Value getFieldType(RecordFieldType&&, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value{"unknown"};
}

template<>
inline rapidjson::Value getFieldType(const std::string&, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value{"std::string"};
}

template<>
inline rapidjson::Value getFieldType(const double&, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value{"double"};
}

template<>
inline rapidjson::Value getFieldType(const int64_t&, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value{"int64_t"};
}

template<>
inline rapidjson::Value getFieldType(const bool&, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value{"bool"};
}

template<>
inline rapidjson::Value getFieldType(const std::chrono::system_clock::time_point&, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value{"std::chrono::system_clock::time_point"};
}

template<>
rapidjson::Value getFieldType(const RecordArray& record_array, rapidjson::Document::AllocatorType& alloc);

template<>
rapidjson::Value getFieldType(const RecordObject& record_object, rapidjson::Document::AllocatorType& alloc);

template<>
inline rapidjson::Value getFieldType(const RecordArray& record_array, rapidjson::Document::AllocatorType& alloc) {
  auto field_type = rapidjson::Value(rapidjson::kArrayType);
  for (const auto& record : record_array) {
    auto json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return getFieldType(f, alloc); }, record.value_));
    field_type.PushBack(json_value, alloc);
  }
  return field_type;
}

template<>
inline rapidjson::Value getFieldType(const RecordObject& record_object, rapidjson::Document::AllocatorType& alloc) {
  auto field_type = rapidjson::Value(rapidjson::kObjectType);
  for (const auto& [record_name, record_value] : record_object) {
    auto json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return getFieldType(f, alloc); }, record_value.value_));
    rapidjson::Value json_name(record_name.c_str(), record_name.length(), alloc);
    field_type.AddMember(json_name, json_value, alloc);
  }
  return field_type;
}

}  // namespace org::apache::nifi::minifi::core
