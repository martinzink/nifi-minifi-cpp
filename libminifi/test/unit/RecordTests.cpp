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
#include <variant>

#include "core/Record.h"
#include "controllers/RecordSetWriter.h"
#include "../TestBase.h"
#include "../Catch.h"


namespace org::apache::nifi::minifi::core::test {

class Fixture {
public:
  explicit Fixture(TestController::PlanConfig config = {}): plan_config_(std::move(config)) {}

  minifi::core::ProcessSession &processSession() { return *process_session_; }

private:
  TestController test_controller_;
  TestController::PlanConfig plan_config_;
  std::shared_ptr<TestPlan> test_plan_ = test_controller_.createPlan(plan_config_);
  std::shared_ptr<minifi::core::Processor> dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
  std::shared_ptr<minifi::core::ProcessContext> context_ = [this] { test_plan_->runNextProcessor(); return test_plan_->getCurrentContext(); }();
  std::unique_ptr<minifi::core::ProcessSession> process_session_ = std::make_unique<core::ProcessSession>(context_);
};


template<typename T>
void templated_func(const std::string& name, T&& t) {
  fmt::print("{} : {}\n", name, t);
}

template<>
void templated_func(const std::string& name, const std::monostate&) {
  fmt::print("{} : null\n", name);
}

template<>
void templated_func(const std::string& name, const RecordObject& ro);

template<>
void templated_func(const std::string& name, const RecordArray& ra) {
  size_t counter = 0;
  for (const auto& element : ra) {
    std::visit([&name, &counter](auto&& f){ templated_func(name + "." + std::to_string(counter++), f); }, element.value_);
  }}

template<>
void templated_func(const std::string& name, const RecordObject& ro) {
  for (const auto& [element_name, element_val] : ro) {
    std::visit([&name, &e=element_name](auto&& f){ templated_func(name + "." + e, f); }, element_val.value_);
  }
}

Record createSampleRecord() {
  Record record;
  record["mono"] = RecordField{.value_ {}};

  record["now"] = RecordField{.value_ = std::chrono::system_clock::now()};
  record["foo"] = RecordField{.value_ = "asd"};
  record["bar"] = RecordField{.value_ = int64_t{123}};
  record["baz"] = RecordField{.value_ = 3.14};
  RecordArray qux;
  qux.push_back(RecordField{.value_ = true});
  qux.push_back(RecordField{.value_ = false});
  qux.push_back(RecordField{.value_ = true});
  RecordObject quux;
  quux["apfel"] = RecordField{.value_ = "apple"};
  quux["birne"] = RecordField{.value_ = "pear"};
  quux["aprikose"] = RecordField{.value_ = "apricot"};

  record["qux"] = RecordField{.value_=qux};
  record["quux"] = RecordField{.value_=quux};
  return record;
}

TEST_CASE("RecordTests") {
  Record record = createSampleRecord();

  for (const auto& [name, field] : record) {
    std::visit([&a = name](auto&& f){ templated_func(a, f); }, field.value_);
  }
}

class SimpleRecordSetWriter final : public RecordSetWriter {
public:
  bool supportsDynamicProperties() const override { return true; }
  void yield() override {}
  bool isRunning() const override { return true;}
  bool isWorkAvailable() override { return true;}

private:
  template<typename T>
  void templated_func(const std::string& name, T&& t) {
    fmt::print("{} : {}\n", name, t);
  }

  template<>
  void templated_func(const std::string& name, const std::monostate&) {
    fmt::print("{} : null\n", name);
  }

  template<>
  void templated_func(const std::string& name, const RecordObject& ro);

  template<>
  void templated_func(const std::string& name, const RecordArray& ra) {
    size_t counter = 0;
    for (const auto& element : ra) {
      std::visit([&name, &counter](auto&& f){ templated_func(name + "." + std::to_string(counter++), f); }, element.value_);
    }}

  template<>
  void templated_func(const std::string& name, const RecordObject& ro) {
    for (const auto& [element_name, element_val] : ro) {
      std::visit([&name, &e=element_name](auto&& f){ templated_func(name + "." + e, f); }, element_val.value_);
    }
  }

 public:

  void write(const RecordSet& record_set, FlowFile& flow_file, ProcessSession& session) override {

  }
};

TEST_CASE("SimpleRecordSetWriter test") {
  Fixture fixture;
  ProcessSession& process_session = fixture.processSession();

  const auto flow_file = process_session.create();

  RecordSet record_set;
  record_set.push_back(createSampleRecord());
  SimpleRecordSetWriter simple_record_set_writer;

  simple_record_set_writer.write(record_set, *flow_file, process_session);
}
}  // namespace org::apache::nifi::minifi::core::test
