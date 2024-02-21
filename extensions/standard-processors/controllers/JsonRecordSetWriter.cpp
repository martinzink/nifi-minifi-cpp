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

#include "JsonRecordSetWriter.h"

namespace org::apache::nifi::minifi::standard {

namespace {

template<typename RecordFieldType>
rapidjson::Value toJson(RecordFieldType&& field, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value(field);
};

template<>
rapidjson::Value toJson(const std::string& str_field, rapidjson::Document::AllocatorType& alloc) {
  return rapidjson::Value(str_field.c_str(), str_field.length(), alloc);
}

template<>
rapidjson::Value toJson(const std::chrono::system_clock::time_point& time_point_field, rapidjson::Document::AllocatorType& alloc) {
  const std::string serialized_time_point = utils::timeutils::getDateTimeStr(std::chrono::floor<std::chrono::seconds>(time_point_field));
  return rapidjson::Value(serialized_time_point.c_str(), serialized_time_point.length(), alloc);
}

template<>
rapidjson::Value toJson(const core::RecordArray& record_array, rapidjson::Document::AllocatorType&);

template<>
rapidjson::Value toJson(const core::RecordObject& record_object, rapidjson::Document::AllocatorType&);

template<>
rapidjson::Value toJson(const core::RecordArray& record_array, rapidjson::Document::AllocatorType& alloc) {
  auto array_json = rapidjson::Value(rapidjson::kArrayType);
  for (const auto& [value_] : record_array) {
    auto json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return toJson(f, alloc); }, value_));
    array_json.PushBack(json_value, alloc);
  }
  return array_json;
}

template<>
rapidjson::Value toJson(const core::RecordObject& record_object, rapidjson::Document::AllocatorType& alloc) {
  auto object_json = rapidjson::Value(rapidjson::kObjectType);
  for (const auto& [record_name, record_value] : record_object) {
    auto json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return toJson(f, alloc); }, record_value.value_));
    rapidjson::Value json_name(record_name.c_str(), record_name.length(), alloc);
    object_json.AddMember(json_name, json_value, alloc);
  }
  return object_json;
}
}  // namespace

void JsonRecordSetWriter::write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  session.write(flow_file, [this, &record_set](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
    auto write_result = 0;
    for (const auto& record : record_set) {
      auto doc = rapidjson::Document(rapidjson::kObjectType);
      auto& allocator = doc.GetAllocator();
      convertRecord(record, doc, allocator);
      rapidjson::StringBuffer buffer;
      rapidjson::Writer writer(buffer);
      doc.Accept(writer);
      write_result += stream->write(gsl::make_span(fmt::format("{}\n", buffer.GetString())).as_span<const std::byte>());
    }
    return write_result;
  });
}

void JsonRecordSetWriter::convertRecord(const core::Record& record, rapidjson::Value& record_json, rapidjson::Document::AllocatorType& alloc) {
  for (const auto& [field_name, field_val] : record) {
    rapidjson::Value json_name(field_name.c_str(), field_name.length(), alloc);
    rapidjson::Value json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return toJson(f, alloc); }, field_val.value_));
    record_json.AddMember(json_name, json_value, alloc);
  }
}


}  // namespace org::apache::nifi::minifi::standard
