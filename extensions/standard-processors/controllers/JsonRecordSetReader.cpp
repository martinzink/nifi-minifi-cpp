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

#include "JsonRecordSetReader.h"

namespace org::apache::nifi::minifi::standard {

namespace {
const rapidjson::Value* getSchemaElement(const rapidjson::Value* const schema, const char* key) {
  if (!schema)
    return nullptr;
  const auto element_schema_itr = schema->FindMember(key);
  if (element_schema_itr == schema->MemberEnd())
    return nullptr;
  return &(element_schema_itr->value);
}

nonstd::expected<core::RecordField, std::error_code> parse(rapidjson::Value& json_value, const rapidjson::Value* const schema) {
  if (json_value.IsDouble()) {
    return core::RecordField{.value_ = json_value.GetDouble()};
  }
  if (json_value.IsBool()) {
    return core::RecordField{.value_ = json_value.GetBool()};
  }
  if (json_value.IsInt64()) {
    return core::RecordField{.value_ = json_value.GetInt64()};
  }
  if (json_value.IsString()) {
    auto json_str = json_value.GetString();
    if (schema && schema->IsString() && std::string_view{schema->GetString()} == "std::chrono::system_clock::time_point") {
      if (auto parsed_time = utils::timeutils::parseRfc3339(json_str)) {
        return core::RecordField{.value_ = *parsed_time};
      }
    }

    return core::RecordField{.value_ = json_str};
  }
  if (json_value.IsArray()) {
    core::RecordArray record_array;
    for (auto itr = json_value.Begin(); itr != json_value.End(); ++itr) {
      const rapidjson::Value* element_schema = nullptr;
      if (schema && schema->IsArray())
        element_schema = schema->Begin();
      auto element_field = parse(*itr, element_schema);
      if (!element_field)
        return nonstd::make_unexpected(element_field.error());
      record_array.push_back(*element_field);
    }
    return core::RecordField{.value_ = record_array};
  }
  if (json_value.IsObject()) {
    core::RecordObject record_object;
    for (auto itr = json_value.MemberBegin(); itr != json_value.MemberEnd(); ++itr) {
      auto element_key = itr->name.GetString();
      auto schema_element = getSchemaElement(schema, element_key);
      auto element_field = parse(itr->value, schema_element);
      if (!element_field)
        return nonstd::make_unexpected(element_field.error());
      record_object[element_key] = *element_field;
    }
    return core::RecordField{.value_ = record_object};
  }

  return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
}

nonstd::expected<core::Record, std::error_code> parseDocument(rapidjson::Document& document, const core::RecordSchema* const record_schema) {
  core::Record result;
  if (!document.IsObject()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  for (auto itr = document.MemberBegin(); itr != document.MemberEnd(); ++itr) {
    const auto element_key = itr->name.GetString();
    const auto element_schema = getSchemaElement(record_schema, element_key);
    auto element_field = parse(itr->value, element_schema);
    if (!element_field)
      return nonstd::make_unexpected(element_field.error());
    result[element_key] = *element_field;
  }
  return result;
}
}  // namespace

nonstd::expected<core::RecordSet, std::error_code> JsonRecordSetReader::read(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session, const core::RecordSchema* const record_schema) {
  core::RecordSet record_set{};
  session.read(flow_file, [&record_set, &record_schema](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    std::string content;
    content.resize(input_stream->size());
    const int64_t read_ret = input_stream->read(as_writable_bytes(std::span(content)));
    if (io::isError(read_ret)) {
      return -1;
    }
    std::stringstream ss(content);
    std::string line;
    while(std::getline(ss, line,'\n')){
      rapidjson::Document document;
      rapidjson::ParseResult parse_result = document.Parse<rapidjson::kParseStopWhenDoneFlag>(content.data());
      if (parse_result.IsError())
        return -1;
      auto record = parseDocument(document, record_schema);
      if (!record)
        return -1;
      record_set.push_back(std::move(*record));
    }
    return read_ret;
  });
  return record_set;
}

}  // namespace org::apache::nifi::minifi::standard