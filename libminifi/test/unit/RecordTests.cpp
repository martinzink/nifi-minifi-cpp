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
#include "../TestRecord.h"


namespace org::apache::nifi::minifi::core::test {

TEST_CASE("RecordField tests") {
  auto record_field_one = RecordField{.value_ = "foo"};
  auto record_field_two = RecordField{.value_ =  "bar"};

  CHECK(record_field_one == record_field_two);
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
