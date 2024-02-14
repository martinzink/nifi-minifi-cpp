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

#include <numbers>
#include <variant>

#include "Catch.h"
#include "TestBase.h"
#include "TestRecord.h"
#include "RecordSetTesters.h"
#include "controllers/JsonRecordSetWriter.h"
#include "controllers/JsonRecordSetReader.h"
#include "core/Record.h"

namespace org::apache::nifi::minifi::standard::test {

TEST_CASE("JsonRecordSetWriter test") {
  core::RecordSet record_set;
  record_set.push_back(core::test::createSampleRecord());
  record_set.push_back(core::test::createSampleRecord2());

  constexpr std::string_view expected =
    R"({"baz":3.14,"qux":[true,false,true],"is_test":true,"bar":123,"quux":{"Apfel":"apple","Birne":"pear","Aprikose":"apricot"},"foo":"asd","when":"2012-07-01T09:53:00Z"}
{"baz":3.141592653589793,"qux":[false,false,true],"is_test":true,"bar":98402134,"quux":{"Apfel":"pomme","Birne":"poire","Aprikose":"abricot"},"foo":"Lorem ipsum dolor sit amet, consectetur adipiscing elit.","when":"2022-11-01T19:52:11Z"}
)";

  JsonRecordSetWriter json_record_set_writer;
  CHECK(core::test::testRecordWriter(json_record_set_writer, record_set, expected));
}

TEST_CASE("JsonRecordSetReader test") {
  core::RecordSet expected_record_set;
  expected_record_set.push_back(core::test::createSampleRecord());
  expected_record_set.push_back(core::test::createSampleRecord2());

  constexpr std::string_view serialized_record_set = R"({"baz":3.14,"qux":[true,false,true],"is_test":true,"bar":123,"quux":{"Apfel":"apple","Birne":"pear","Aprikose":"apricot"},"foo":"asd","when":"2012-07-01T09:53:00Z"}
{"baz":3.141592653589793,"qux":[false,false,true],"is_test":true,"bar":98402134,"quux":{"Apfel":"pomme","Birne":"poire","Aprikose":"abricot"},"foo":"Lorem ipsum dolor sit amet, consectetur adipiscing elit.","when":"2022-11-01T19:52:11Z"}
)";

  JsonRecordSetReader json_record_set_reader;
  const auto record_schema = expected_record_set[0].getSchema();
  CHECK(core::test::testRecordReader(json_record_set_reader, serialized_record_set, expected_record_set, &record_schema));
}
}  // namespace org::apache::nifi::minifi::standard::test
