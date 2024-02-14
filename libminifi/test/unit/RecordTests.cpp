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
 * limitations under the License.c
 */


#include "../Catch.h"
#include "../TestBase.h"
#include "core/Record.h"


namespace org::apache::nifi::minifi::core::test {

Record createSampleRecord() {
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  Record record;

  record["when"] = RecordField{.value_ = date::sys_days(2012_y / 07 / 01) + 9h + 53min + 00s};
  record["foo"] = RecordField{.value_ = "asd"};
  record["bar"] = RecordField{.value_ = int64_t{123}};
  record["baz"] = RecordField{.value_ = 3.14};
  record["is_test"] = RecordField{.value_ = true};
  RecordArray qux;
  qux.push_back(RecordField{.value_ = true});
  qux.push_back(RecordField{.value_ = false});
  qux.push_back(RecordField{.value_ = true});
  RecordObject quux;
  quux["Apfel"] = RecordField{.value_ = "apple"};
  quux["Birne"] = RecordField{.value_ = "pear"};
  quux["Aprikose"] = RecordField{.value_ = "apricot"};

  record["qux"] = RecordField{.value_=qux};
  record["quux"] = RecordField{.value_=quux};
  return record;
}

TEST_CASE("RecordSchema test") {
  auto record = createSampleRecord();
  auto schema = record.getSchema();

  rapidjson::StringBuffer buffer;
  rapidjson::Writer writer(buffer);
  schema.Accept(writer);
  std::string schema_str{buffer.GetString()};

  CHECK(schema_str == R"({"baz":"double","qux":["bool","bool","bool"],"is_test":"bool","bar":"int64_t","quux":{"Apfel":"std::string","Birne":"std::string","Aprikose":"std::string"},"foo":"std::string","when":"std::chrono::system_clock::time_point"})");
}
}  // namespace org::apache::nifi::minifi::core::test
