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
#include "controllers/JsonRecordSetWriter.h"
#include "controllers/JsonRecordSetReader.h"
#include "core/Record.h"

namespace org::apache::nifi::minifi::standard::test {

class Fixture {
public:
  explicit Fixture(TestController::PlanConfig config = {}): plan_config_(std::move(config)) {}

  core::ProcessSession &processSession() { return *process_session_; }

private:
  TestController test_controller_;
  TestController::PlanConfig plan_config_;
  std::shared_ptr<TestPlan> test_plan_ = test_controller_.createPlan(plan_config_);
  std::shared_ptr<core::Processor> dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
  std::shared_ptr<core::ProcessContext> context_ = [this] { test_plan_->runNextProcessor(); return test_plan_->getCurrentContext(); }();
  std::unique_ptr<core::ProcessSession> process_session_ = std::make_unique<core::ProcessSession>(context_);
};

const core::Relationship Success{"success", "everything is fine"};

core::Record createSampleRecord2() {
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  using core::RecordField, core::RecordArray, core::RecordObject;
  core::Record record;

  record["when"] = RecordField{.value_ = date::sys_days(2022_y / 11 / 01) + 19h + 52min + 11s};
  record["foo"] = RecordField{.value_ = "Lorem ipsum dolor sit amet, consectetur adipiscing elit."};
  record["bar"] = RecordField{.value_ = int64_t{98402134}};
  record["baz"] = RecordField{.value_ = std::numbers::pi};
  record["is_test"] = RecordField{.value_ = true};
  RecordArray qux;
  qux.push_back(RecordField{.value_ = false});
  qux.push_back(RecordField{.value_ = false});
  qux.push_back(RecordField{.value_ = true});
  RecordObject quux;
  quux["Apfel"] = RecordField{.value_ = "pomme"};
  quux["Birne"] = RecordField{.value_ = "poire"};
  quux["Aprikose"] = RecordField{.value_ = "abricot"};

  record["qux"] = RecordField{.value_=qux};
  record["quux"] = RecordField{.value_=quux};
  return record;
}

core::Record createSampleRecord() {
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  using core::RecordField, core::RecordArray, core::RecordObject;
  core::Record record;

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

TEST_CASE("JsonRecordSetWriter test") {
  Fixture fixture;
  core::ProcessSession& process_session = fixture.processSession();

  const auto flow_file = process_session.create();

  core::RecordSet record_set;
  record_set.push_back(createSampleRecord());
  record_set.push_back(createSampleRecord2());
  JsonRecordSetWriter simple_record_set_writer;

  simple_record_set_writer.write(record_set, flow_file, process_session);
  process_session.transfer(flow_file, Success);
  process_session.commit();
  auto input_stream = process_session.getFlowFileContentStream(flow_file);
  std::array<std::byte, 2048> buffer{};
  auto buffer_size = input_stream->read(buffer);
  std::string flow_file_content((char *)buffer.data(), buffer_size);
  std::string expected =
    R"({"baz":3.14,"qux":[true,false,true],"is_test":true,"bar":123,"quux":{"Apfel":"apple","Birne":"pear","Aprikose":"apricot"},"foo":"asd","when":"2012-07-01T09:53:00Z"}
{"baz":3.141592653589793,"qux":[false,false,true],"is_test":true,"bar":98402134,"quux":{"Apfel":"pomme","Birne":"poire","Aprikose":"abricot"},"foo":"Lorem ipsum dolor sit amet, consectetur adipiscing elit.","when":"2022-11-01T19:52:11Z"}
)";
  CHECK(flow_file_content == expected);
}

TEST_CASE("JsonRecordSetReader test") {
  Fixture fixture;
  core::ProcessSession& process_session = fixture.processSession();

  const auto flow_file = process_session.create();
  process_session.writeBuffer(flow_file, R"({"pi":3.14,"tp":"2022-11-01T19:52:11Z"})");
  process_session.transfer(flow_file, Success);
  process_session.commit();
  JsonRecordSetReader reader;

  core::RecordSchema record_schema;
  rapidjson::ParseResult parse_result = record_schema.Parse<rapidjson::kParseStopWhenDoneFlag>(R"({"pi":"double","tp":"std::chrono::system_clock::time_point"})");
  REQUIRE(!parse_result.IsError());

  const auto record_set = reader.read(flow_file, process_session, &record_schema);
  REQUIRE(record_set.has_value());

  REQUIRE(record_set->size() == 1);
  const auto& record = record_set->at(0);
  auto [pi_field] = record["pi"];
  auto [tp_field] = record["tp"];
  REQUIRE(std::holds_alternative<double>(pi_field));
  REQUIRE(std::holds_alternative<std::chrono::system_clock::time_point>(tp_field));
  CHECK(std::get<double>(pi_field) == 3.14);
  CHECK(std::get<std::chrono::system_clock::time_point>(tp_field) == *utils::timeutils::parseRfc3339("2022-11-01T19:52:11Z"));
}
}  // namespace org::apache::nifi::minifi::standard::test
