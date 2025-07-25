/**
 *
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

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>

#include "FlowController.h"
#include "LogAttribute.h"
#include "TailFile.h"
#include "TextFragmentUtils.h"
#include "catch2/generators/catch_generators.hpp"
#include "core/Core.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Processor.h"
#include "core/Resource.h"
#include "unit/Catch.h"
#include "unit/ProvenanceTestHelper.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

using namespace std::literals::chrono_literals;

static constexpr std::string_view NEWLINE_FILE = ""  // NOLINT
        "one,two,three\n"
        "four,five,six, seven";
static constexpr std::string_view TMP_FILE = "minifi-tmpfile.txt";
static constexpr std::string_view ROLLED_OVER_TMP_FILE = "minifi-tmpfile.txt.old.1";
static constexpr std::string_view STATE_FILE = "minifi-state-file.txt";
static constexpr std::string_view ROLLED_OVER_TAIL_DATA = "rolled_over_data\n";
static constexpr std::string_view NEW_TAIL_DATA = "newdata\n";
static constexpr std::string_view ADDITIONALY_CREATED_FILE_CONTENT = "additional file data\n";

namespace {
std::filesystem::path createTempFile(const std::filesystem::path& directory, const std::filesystem::path& file_name,
    const std::string_view contents, const std::ios_base::openmode open_mode = std::ios::out | std::ios::binary,
    const std::chrono::file_clock::duration offset = 0ms) {
  if (!utils::file::exists(directory)) {
    std::filesystem::create_directories(directory);
  }
  auto full_file_name = directory / file_name;
  std::ofstream{full_file_name, open_mode} << contents;
  std::filesystem::last_write_time(full_file_name, std::chrono::file_clock::now() + offset);

  return full_file_name;
}

void appendTempFile(const std::filesystem::path& directory, const std::filesystem::path& file_name,
    const std::string_view contents, const std::ios_base::openmode open_mode = std::ios::app | std::ios::binary) {
  createTempFile(directory, file_name, contents, open_mode);
}

}  // namespace

TEST_CASE("TailFile reads the file until the first delimiter then picks up the second line if a delimiter is written between runs", "[simple]") {
  // Create and write to the test file
  minifi::test::SingleProcessorTestController test_controller(std::make_unique<minifi::processors::TailFile>("TailFile"));

  auto dir = test_controller.createTempDirectory();
  auto temp_file_path = dir / TMP_FILE;

  {
    std::ofstream tmp_file;
    tmp_file.open(temp_file_path, std::ios::out | std::ios::binary);
    tmp_file << NEWLINE_FILE;
  }

  test_controller.plan->setProperty(test_controller.getProcessor(), minifi::processors::TailFile::FileName, temp_file_path.string());
  test_controller.plan->setProperty(test_controller.getProcessor(), minifi::processors::TailFile::Delimiter, "\n");

  {
    auto trigger_res = test_controller.trigger();
    const auto& success_ffs = trigger_res.at(minifi::processors::TailFile::Success);
    REQUIRE(success_ffs.size() == 1);
    CHECK(test_controller.plan->getContent(success_ffs[0]) == "one,two,three\n");
    CHECK(success_ffs[0]->getAttribute(minifi::processors::textfragmentutils::BASE_NAME_ATTRIBUTE) == "minifi-tmpfile");
    CHECK(success_ffs[0]->getAttribute(minifi::processors::textfragmentutils::POST_NAME_ATTRIBUTE) == "txt");
    CHECK(success_ffs[0]->getAttribute(minifi::processors::textfragmentutils::OFFSET_ATTRIBUTE) == "0");
    CHECK(success_ffs[0]->getAttribute(core::SpecialFlowAttribute::PATH) == dir.string());
    CHECK(success_ffs[0]->getAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH) == temp_file_path.string() );
    CHECK(success_ffs[0]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "minifi-tmpfile.0-13.txt");
  }
  {
    std::ofstream appendStream;
    appendStream.open(temp_file_path, std::ios_base::app | std::ios_base::binary);
    appendStream << std::endl;
  }
  {
    auto trigger_res = test_controller.trigger();
    const auto& success_ffs = trigger_res.at(minifi::processors::TailFile::Success);
    REQUIRE(success_ffs.size() == 1);
    CHECK(test_controller.plan->getContent(success_ffs[0]) == "four,five,six, seven\n");
    CHECK(success_ffs[0]->getAttribute(minifi::processors::textfragmentutils::BASE_NAME_ATTRIBUTE) == "minifi-tmpfile");
    CHECK(success_ffs[0]->getAttribute(minifi::processors::textfragmentutils::POST_NAME_ATTRIBUTE) == "txt");
    CHECK(success_ffs[0]->getAttribute(minifi::processors::textfragmentutils::OFFSET_ATTRIBUTE) == "14");
    CHECK(success_ffs[0]->getAttribute(core::SpecialFlowAttribute::PATH) == dir.string());
    CHECK(success_ffs[0]->getAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH) == temp_file_path.string() );
    CHECK(success_ffs[0]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "minifi-tmpfile.14-34.txt");
  }
}

TEST_CASE("TailFile re-reads the file if the state is deleted between runs", "[state]") {
  // Create and write to the test file

  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();
  auto temp_file_path = dir / TMP_FILE;

  {
    std::ofstream tmp_file;
    tmp_file.open(temp_file_path, std::ios::out | std::ios::binary);
    tmp_file << NEWLINE_FILE;
  }

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  TestController::runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.0-13.txt"));

  plan->reset(true);  // start a new but with state file
  LogTestController::getInstance().clear();

  plan->getProcessContextForProcessor(tailfile)->getStateManager()->clear();

  TestController::runSession(plan, true);

  // if we lose state we restart
  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.0-13.txt"));
}

TEST_CASE("TailFile picks up the state correctly if it is rewritten between runs", "[state]") {
  // Create and write to the test file
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();
  auto temp_file_path = dir / TMP_FILE;

  std::ofstream tmpfile;
  tmpfile.open(temp_file_path, std::ios::out | std::ios::binary);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  std::ofstream appendStream;
  appendStream.open(temp_file_path, std::ios_base::app | std::ios_base::binary);
  appendStream.write("\n", 1);
  appendStream.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.0-13.txt"));

  REQUIRE(temp_file_path.has_filename());
  REQUIRE(temp_file_path.has_parent_path());

  // should stay the same
  for (int i = 0; i < 5; i++) {
    plan->reset(true);  // start a new but with state file
    LogTestController::getInstance().clear();

    plan->getProcessContextForProcessor(tailfile)->getStateManager()->set({{"file.0.name", temp_file_path.filename().string()},
                                                                                   {"file.0.position", "14"},
                                                                                   {"file.0.current", temp_file_path.string()}});

    TestController::runSession(plan, true);

    // if we lose state we restart
    REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.14-34.txt"));
  }
  for (int i = 14; i < 34; i++) {
    plan->reset(true);  // start a new but with state file

    plan->getProcessContextForProcessor(tailfile)->getStateManager()->set({{"file.0.name", temp_file_path.filename().string()},
                                                                                   {"file.0.position", std::to_string(i)},
                                                                                   {"file.0.current", temp_file_path.string()}});

    TestController::runSession(plan, true);
  }

  plan->runCurrentProcessor();
  for (int i = 14; i < 34; i++) {
    REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile." + std::to_string(i) + "-34.txt"));
  }
}

TEST_CASE("TailFile converts the old-style state file to the new-style state", "[state][migration]") {
  // Create and write to the test file
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto id = tailfile->getUUIDStr();

  auto logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");

  auto dir = testController.createTempDirectory();
  auto state_file_path = dir / STATE_FILE;

  auto new_state_file_path = state_file_path.string() + ("." + id);

  SECTION("single") {
    const auto temp_file = createTempFile(dir, TMP_FILE, std::string(NEWLINE_FILE) + '\n');

    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file.string());
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile, state_file_path.string());
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

    std::ofstream new_state_file;
    new_state_file.open(new_state_file_path);
    SECTION("legacy") {
      new_state_file << "FILENAME=" << temp_file.string() << std::endl;
      new_state_file << "POSITION=14" << std::endl;
    }
    SECTION("newer single") {
      new_state_file << "FILENAME=" << TMP_FILE << std::endl;
      new_state_file << "POSITION." << TMP_FILE << "=14" << std::endl;
      new_state_file << "CURRENT." << TMP_FILE << "=" << temp_file.string() << std::endl;
    }
    new_state_file.close();

    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("key:filename value:minifi-tmpfile.14-34.txt"));

    std::unordered_map<std::string, std::string> state;
    REQUIRE(plan->getProcessContextForProcessor(tailfile)->getStateManager()->get(state));

    REQUIRE(temp_file.has_filename());
    REQUIRE(temp_file.has_parent_path());
    std::unordered_map<std::string, std::string> expected_state{{"file.0.name", temp_file.filename().string()},
                                                                {"file.0.position", "35"},
                                                                {"file.0.current", temp_file.string()},
                                                                {"file.0.checksum", "1404369522"}};
    for (const auto& key_value_pair : expected_state) {
      const auto it = state.find(key_value_pair.first);
      REQUIRE(it != state.end());
      REQUIRE(it->second == key_value_pair.second);
    }
    REQUIRE(state.find("file.0.last_read_time") != state.end());
  }

  SECTION("multiple") {
    const std::string file_name_1 = "bar.txt";
    const std::string file_name_2 = "foo.txt";
    const auto temp_file_1 = createTempFile(dir, file_name_1, std::string(NEWLINE_FILE) + '\n');
    const auto temp_file_2 = createTempFile(dir, file_name_2, std::string(NEWLINE_FILE) + '\n');

    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory, dir.string());
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency, "0 sec");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, ".*\\.txt");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::StateFile, state_file_path.string());
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

    std::ofstream newstatefile;
    newstatefile.open(new_state_file_path);
    newstatefile << "FILENAME=" << file_name_1 << std::endl;
    newstatefile << "POSITION." << file_name_1 << "=14" << std::endl;
    newstatefile << "CURRENT." << file_name_1 << "=" << temp_file_1.string() << std::endl;
    newstatefile << "FILENAME=" << file_name_2 << std::endl;
    newstatefile << "POSITION." << file_name_2 << "=15" << std::endl;
    newstatefile << "CURRENT." << file_name_2 << "=" << temp_file_2.string() << std::endl;
    newstatefile.close();

    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains(file_name_1.substr(0, file_name_1.rfind('.')) + ".14-34.txt"));
    REQUIRE(LogTestController::getInstance().contains(file_name_2.substr(0, file_name_2.rfind('.')) + ".15-34.txt"));

    std::unordered_map<std::string, std::string> state;
    REQUIRE(plan->getProcessContextForProcessor(tailfile)->getStateManager()->get(state));

    REQUIRE(temp_file_1.has_parent_path());
    REQUIRE(temp_file_1.has_filename());
    REQUIRE(temp_file_2.has_parent_path());
    REQUIRE(temp_file_2.has_filename());
    std::unordered_map<std::string, std::string> expected_state{{"file.0.name", temp_file_1.filename().string()},
                                                                {"file.0.position", "35"},
                                                                {"file.0.current", temp_file_1.string()},
                                                                {"file.0.checksum", "1404369522"},
                                                                {"file.1.name", temp_file_2.filename().string()},
                                                                {"file.1.position", "35"},
                                                                {"file.1.current", temp_file_2.string()},
                                                                {"file.1.checksum", "2289158555"}};
    for (const auto& key_value_pair : expected_state) {
      const auto it = state.find(key_value_pair.first);
      REQUIRE(it != state.end());
      REQUIRE(it->second == key_value_pair.second);
    }
    REQUIRE(state.find("file.0.last_read_time") != state.end());
    REQUIRE(state.find("file.1.last_read_time") != state.end());
  }
}

TEST_CASE("TailFile picks up the new File to Tail if it is changed between runs", "[state]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tail_file = plan->addProcessor("TailFile", "tail_file");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  auto log_attribute = plan->addProcessor("LogAttribute", "log_attribute", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");

  auto directory = testController.createTempDirectory();
  auto first_test_file = createTempFile(directory, "first.log", "my first log line\n");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName, first_test_file.string());
  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:first.0-17.log"));

  SECTION("The new file gets picked up") {
    auto second_test_file = createTempFile(directory, "second.log", "my second log line\n");
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName, second_test_file.string());
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().clear();
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:second.0-18.log"));
  }

  SECTION("The old file will no longer be tailed") {
    appendTempFile(directory, "first.log", "add some more stuff\n");
    auto second_test_file = createTempFile(directory, "second.log", "");
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName, second_test_file.string());
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().clear();
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 0 flow files"));
  }
}

TEST_CASE("TailFile picks up the new File to Tail if it is changed between runs (multiple file mode)", "[state][multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  auto directory = testController.createTempDirectory();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tail_file = plan->addProcessor("TailFile", "tail_file");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::BaseDirectory, directory.string());
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::LookupFrequency, "0 sec");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName, "first\\..*\\.log");

  auto log_attribute = plan->addProcessor("LogAttribute", "log_attribute", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");

  createTempFile(directory, "first.fruit.log", "apple\n");
  createTempFile(directory, "second.fruit.log", "orange\n");
  createTempFile(directory, "first.animal.log", "hippopotamus\n");
  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:first.fruit.0-5.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:first.animal.0-12.log"));

  appendTempFile(directory, "first.fruit.log", "banana\n");
  appendTempFile(directory, "first.animal.log", "hedgehog\n");

  SECTION("If a file no longer matches the new regex, then we stop tailing it") {
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName, "first\\.f.*\\.log");
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().clear();
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:first.fruit.6-12.log"));
  }

  SECTION("If a new file matches the new regex, we start tailing it") {
    plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::FileName, ".*\\.fruit\\.log");
    plan->reset(true);  // clear the memory, but keep the state file
    LogTestController::getInstance().clear();
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow file"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:first.fruit.6-12.log"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:second.fruit.0-6.log"));
  }
}

TEST_CASE("TailFile finds the single input file in both Single and Multiple mode", "[simple]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "");

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();
  auto temp_file_path = dir / TMP_FILE;
  std::ofstream tmpfile;
  tmpfile.open(temp_file_path, std::ios::out | std::ios::binary);
  tmpfile << NEWLINE_FILE;
  tmpfile.close();

  SECTION("Single") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  }

  SECTION("Multiple") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, "minifi-.*\\.txt");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory, dir.string());
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency, "0 sec");
  }

  TestController::runSession(plan, false);
  auto records = plan->getProvenanceRecords();
  REQUIRE(records.size() == 2);

  TestController::runSession(plan, false);

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.size()) + " Offset:0"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile picks up new files created between runs", "[multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  auto dir = testController.createTempDirectory();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfile");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory, dir.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency, "0 sec");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, ".*\\.log");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  auto logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");

  createTempFile(dir, "application.log", "line1\nline2\n");

  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));

  createTempFile(dir, "another.log", "some more content\n");

  plan->reset();
  LogTestController::getInstance().clear();

  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile can handle input files getting removed", "[multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  auto dir = testController.createTempDirectory();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfile");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory, dir.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::LookupFrequency, "0 sec");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, ".*\\.log");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  auto logattribute = plan->addProcessor("LogAttribute", "logattribute",
                                                                     core::Relationship("success", "description"),
                                                                     true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");

  createTempFile(dir, "one.log", "line one\n");
  createTempFile(dir, "two.log", "some stuff\n");

  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));

  plan->reset();
  LogTestController::getInstance().clear();

  appendTempFile(dir, "one.log", "line two\nline three\nline four\n");
  CHECK(std::filesystem::remove(dir / "two.log"));

  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile processes a very long line correctly", "[simple]") {
  std::string line1("012\n");
  std::string line2(8050, 0);
  std::mt19937 gen(std::random_device{}());  // NOLINT (linter wants a space before '{') [whitespace/braces]
  std::generate_n(line2.begin(), line2.size() - 1, [&] {
    // Make sure to only generate from characters that don't intersect with line1 and 3-4
    // Starting generation from 64 ensures that no numeric digit characters are added
    return gsl::narrow<char>(64 + gen() % (127 - 64));
  });
  line2.back() = '\n';
  std::string line3("345\n");
  std::string line4("6789");

  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");

  auto dir = testController.createTempDirectory();
  auto temp_file_path = dir / TMP_FILE;
  std::ofstream tmpfile;
  tmpfile.open(temp_file_path, std::ios::out | std::ios::binary);
  tmpfile << line1 << line2 << line3 << line4;
  tmpfile.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, minifi::processors::LogAttribute::FlowFilesToLog, "0");
  plan->setProperty(log_attr, minifi::processors::LogAttribute::LogPayload, "true");
  plan->setProperty(log_attr, minifi::processors::LogAttribute::HexencodePayload, "true");

  uint32_t line_length = 0U;
  SECTION("with line length 80") {
    line_length = 80U;
    plan->setProperty(log_attr, minifi::processors::LogAttribute::MaxPayloadLineLength, "80");
  }
  SECTION("with line length 200") {
    line_length = 200U;
    plan->setProperty(log_attr, minifi::processors::LogAttribute::MaxPayloadLineLength, "200");
  }
  SECTION("with line length 0") {
    line_length = 0U;
    plan->setProperty(log_attr, minifi::processors::LogAttribute::MaxPayloadLineLength, "0");
  }
  SECTION("with line length 16") {
    line_length = 16U;
    plan->setProperty(log_attr, minifi::processors::LogAttribute::MaxPayloadLineLength, "16");
  }

  TestController::runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  REQUIRE(LogTestController::getInstance().contains(utils::string::to_hex(line1)));
  auto line2_hex = utils::string::to_hex(line2);
  if (line_length == 0U) {
    REQUIRE(LogTestController::getInstance().contains(line2_hex));
  } else {
    std::stringstream line2_hex_lines;
    for (size_t i = 0; i < line2_hex.size(); i += line_length) {
      line2_hex_lines << line2_hex.substr(i, line_length) << '\n';
    }
    REQUIRE(LogTestController::getInstance().contains(line2_hex_lines.str()));
  }
  REQUIRE(LogTestController::getInstance().contains(utils::string::to_hex(line3)));
  REQUIRE(false == LogTestController::getInstance().contains(utils::string::to_hex(line4), std::chrono::seconds(0)));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile processes a long line followed by multiple newlines correctly", "[simple][edge_case]") {
  // Test having two delimiters on the buffer boundary
  std::string line1(4098, '\n');
  std::mt19937 gen(std::random_device { }());
  std::generate_n(line1.begin(), 4095, [&] {
    // Make sure to only generate from characters that don't intersect with line2-4
    // Starting generation from 64 ensures that no numeric digit characters are added
    return gsl::narrow<char>(64 + gen() % (127 - 64));
  });
  std::string line2("012\n");
  std::string line3("345\n");
  std::string line4("6789");

  // Create and write to the test file

  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto dir = testController.createTempDirectory();
  auto temp_file_path = dir / TMP_FILE;
  std::ofstream tmpfile;
  tmpfile.open(temp_file_path, std::ios::out | std::ios::binary);
  tmpfile << line1 << line2 << line3 << line4;
  tmpfile.close();

  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, minifi::processors::LogAttribute::FlowFilesToLog, "0");
  plan->setProperty(log_attr, minifi::processors::LogAttribute::LogPayload, "true");
  plan->setProperty(log_attr, minifi::processors::LogAttribute::HexencodePayload, "true");
  plan->setProperty(log_attr, minifi::processors::LogAttribute::MaxPayloadLineLength, "80");

  TestController::runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 5 flow files"));
  auto line1_hex = utils::string::to_hex(line1.substr(0, 4096));
  std::stringstream line1_hex_lines;
  for (size_t i = 0; i < line1_hex.size(); i += 80) {
    line1_hex_lines << line1_hex.substr(i, 80) << '\n';
  }
  REQUIRE(LogTestController::getInstance().contains(line1_hex_lines.str()));
  REQUIRE(LogTestController::getInstance().contains(utils::string::to_hex(line2)));
  REQUIRE(LogTestController::getInstance().contains(utils::string::to_hex(line3)));
  REQUIRE(false == LogTestController::getInstance().contains(utils::string::to_hex(line4), std::chrono::seconds(0)));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile onSchedule throws if file(s) to tail cannot be determined", "[configuration]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");

  SECTION("Single file mode by default") {
    SECTION("No FileName") {
    }

    SECTION("FileName does not contain the path") {
      plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, "minifi-log.txt");
    }
  }

  SECTION("Explicit Single file mode") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode, "Single file");

    SECTION("No FileName") {
    }

    SECTION("FileName does not contain the path") {
      plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, "minifi-log.txt");
    }
  }

  SECTION("Multiple file mode") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");

    SECTION("No FileName and no BaseDirectory") {
    }

    SECTION("No BaseDirectory") {
      plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, "minifi-.*\\.txt");
    }
  }

  REQUIRE_THROWS(plan->runNextProcessor());
}

TEST_CASE("TailFile onSchedule throws in Multiple mode if the Base Directory does not exist", "[configuration][multiple_file]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  plan->setProperty(tailfile, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tailfile, minifi::processors::TailFile::FileName, ".*\\.log");

  SECTION("No Base Directory is set") {
    REQUIRE_THROWS(plan->runNextProcessor());
  }

  SECTION("Base Directory is set, but does not exist") {
    std::string nonexistent_file_name{"/no-such-directory/688b01d0-9e5f-11ea-820d-f338c34d39a1/31d1a81a-9e5f-11ea-a77b-8b27d514a452"};
    plan->setProperty(tailfile, minifi::processors::TailFile::BaseDirectory, nonexistent_file_name);
    REQUIRE_THROWS(plan->runNextProcessor());
  }

  SECTION("Base Directory is set and it exists") {
    auto directory = testController.createTempDirectory();

    plan->setProperty(tailfile, minifi::processors::TailFile::BaseDirectory, directory.string());
    plan->setProperty(tailfile, minifi::processors::TailFile::LookupFrequency, "0 sec");
    REQUIRE_NOTHROW(plan->runNextProcessor());
  }
}

TEST_CASE("TailFile finds and finishes the renamed file and continues with the new log file", "[rotation]") {
  minifi::test::SingleProcessorTestController test_controller(std::make_unique<minifi::processors::TailFile>("TailFile"));

  constexpr char DELIM = ',';
  constexpr size_t expected_pieces = std::ranges::count(NEWLINE_FILE, DELIM);  // The last piece is left as considered unfinished

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto dir = test_controller.createTempDirectory();
  auto in_file = dir / "testfifo.txt";

  {
    std::ofstream in_file_stream(in_file, std::ios::out | std::ios::binary);
    in_file_stream << NEWLINE_FILE;
  }
  std::filesystem::last_write_time(in_file, std::chrono::file_clock::now() - 200ms);

  // Build MiNiFi processing graph
  test_controller.plan->setProperty(test_controller.getProcessor(), minifi::processors::TailFile::Delimiter, std::string(1, DELIM));

  SECTION("single") {
    test_controller.plan->setProperty(test_controller.getProcessor(), minifi::processors::TailFile::FileName, in_file.string());
  }
  SECTION("Multiple") {
    test_controller.plan->setProperty(test_controller.getProcessor(), org::apache::nifi::minifi::processors::TailFile::FileName, "testfifo.txt");
    test_controller.plan->setProperty(test_controller.getProcessor(), org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");
    test_controller.plan->setProperty(test_controller.getProcessor(), org::apache::nifi::minifi::processors::TailFile::BaseDirectory, dir.string());
    test_controller.plan->setProperty(test_controller.getProcessor(), org::apache::nifi::minifi::processors::TailFile::LookupFrequency, "0 sec");
  }
  {
    auto res = test_controller.trigger();
    const auto& success_ffs = res.at(minifi::processors::TailFile::Success);
    REQUIRE(success_ffs.size() == expected_pieces);
    CHECK(test_controller.plan->getContent(success_ffs[0]) == "one,");
    CHECK(test_controller.plan->getContent(success_ffs[1]) == "two,");
    CHECK(test_controller.plan->getContent(success_ffs[2]) == "three\nfour,");
    CHECK(test_controller.plan->getContent(success_ffs[3]) == "five,");
    CHECK(test_controller.plan->getContent(success_ffs[4]) == "six,");
  }

  auto rotated_file = in_file;
  rotated_file += ".1";

  REQUIRE_NOTHROW(std::filesystem::rename(in_file, rotated_file));
  std::filesystem::last_write_time(rotated_file, std::chrono::file_clock::now());

  {
    std::ofstream new_in_file_stream(in_file, std::ios::out | std::ios::binary);
    new_in_file_stream << "five" << DELIM << "six" << DELIM;
  }
  std::filesystem::last_write_time(in_file, std::chrono::file_clock::now() - 100ms);

  {
    auto res = test_controller.trigger();
    const auto& success_ffs = res.at(minifi::processors::TailFile::Success);
    CHECK(success_ffs.size() == 3);
    CHECK(success_ffs[0]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "testfifo.txt.28-33.1");
    CHECK(test_controller.plan->getContent(success_ffs[0]) == " seven");
    CHECK(success_ffs[1]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "testfifo.0-4.txt");
    CHECK(test_controller.plan->getContent(success_ffs[1]) == "five,");
    CHECK(success_ffs[2]->getAttribute(core::SpecialFlowAttribute::FILENAME) == "testfifo.5-8.txt");
    CHECK(test_controller.plan->getContent(success_ffs[2]) == "six,");
  };
}

TEST_CASE("TailFile finds and finishes multiple rotated files and continues with the new log file", "[rotation]") {
  minifi::test::SingleProcessorTestController test_controller(std::make_unique<minifi::processors::TailFile>("TailFile"));

  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();

  constexpr char DELIM = ':';

  auto dir = test_controller.createTempDirectory();
  auto fruits_log_path = dir / "fruits.log";

  REQUIRE(test_controller.getProcessor()->setProperty(minifi::processors::TailFile::FileName.name, fruits_log_path.string()));
  REQUIRE(test_controller.getProcessor()->setProperty(minifi::processors::TailFile::Delimiter.name, std::string(1, DELIM)));

  const auto file_modi_time_t0 = std::chrono::file_clock::now();
  {
    std::ofstream test_file_stream(fruits_log_path, std::ios::binary);
    test_file_stream << "Apple" << DELIM << "Orange" << DELIM;
  }
  std::filesystem::last_write_time(fruits_log_path, file_modi_time_t0 - 1s);

  {
    const auto result = test_controller.trigger();
    const auto& success_ffs = result.at(minifi::processors::TailFile::Success);
    REQUIRE(success_ffs.size() == 2);
    CHECK(test_controller.plan->getContent(success_ffs[0]) == "Apple:");
    CHECK(test_controller.plan->getContent(success_ffs[1]) == "Orange:");
  }

  {
    std::ofstream test_file_stream(fruits_log_path, std::ios::binary | std::ios::app);
    test_file_stream << "Pear" << DELIM;
  }

  auto first_rotated_file = dir / "fruits.0.log";
  REQUIRE_NOTHROW(std::filesystem::rename(fruits_log_path, first_rotated_file));

  {
    std::ofstream test_file_stream_1(fruits_log_path, std::ios::binary);
    test_file_stream_1 << "Pineapple" << DELIM << "Kiwi" << DELIM;
  }

  auto second_rotated_file = dir / "fruits.1.log";
  REQUIRE_NOTHROW(std::filesystem::rename(fruits_log_path, second_rotated_file));

  std::ofstream test_file_stream_2(fruits_log_path, std::ios::binary);
  test_file_stream_2 << "Apricot" << DELIM;
  test_file_stream_2.close();

  {
    const auto result = test_controller.trigger();
    const auto& success_ffs = result.at(minifi::processors::TailFile::Success);
    REQUIRE(success_ffs.size() == 4);
    CHECK(test_controller.plan->getContent(success_ffs[0]) == "Pear:");
    CHECK(test_controller.plan->getContent(success_ffs[1]) == "Pineapple:");
    CHECK(test_controller.plan->getContent(success_ffs[2]) == "Kiwi:");
    CHECK(test_controller.plan->getContent(success_ffs[3]) == "Apricot:");
  }
}

TEST_CASE("TailFile ignores old rotated files", "[rotation]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  const auto dir = testController.createTempDirectory();
  auto log_file_name = dir / "test.log";

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfile");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, log_file_name.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  auto logattribute = plan->addProcessor("LogAttribute", "logattribute",
                                                                     core::Relationship("success", "description"),
                                                                     true);
  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");

  createTempFile(dir, "test.2019-08-20", "line1\nline2\nline3\nline4\n", std::ios::out | std::ios::binary, -1s);   // very old rotated file
  createTempFile(dir, "test.log", "line5\nline6\nline7\n");

  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:test.2019-08-20", std::chrono::seconds(0)));

  auto rotated_log_file_name = dir / "test.2020-05-18";
  REQUIRE_NOTHROW(std::filesystem::rename(log_file_name, rotated_log_file_name));

  createTempFile(dir, "test.log", "line8\nline9\n");

  plan->reset();
  LogTestController::getInstance().clear();

  TestController::runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:test.2019-08-20", std::chrono::seconds(0)));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile rotation works with multiple input files", "[rotation][multiple_file]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto dir = testController.createTempDirectory();

  createTempFile(dir, "fruit.log", "apple\npear\nbanana\n", std::ios::out | std::ios::binary, -1100ms);
  createTempFile(dir, "animal.log", "bear\ngiraffe\n", std::ios::out | std::ios::binary, -1100ms);
  createTempFile(dir, "color.log", "red\nblue\nyellow\npurple\n", std::ios::out | std::ios::binary, -1100ms);

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tail_file, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, ".*\\.log");
  plan->setProperty(tail_file, minifi::processors::TailFile::BaseDirectory, dir.string());
  plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "0 sec");

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

  TestController::runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged " + std::to_string(3 + 2 + 4) + " flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.0-5.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.6-10.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.11-17.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.0-4.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.5-12.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.0-3.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.4-8.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.9-15.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.16-22.log"));

  appendTempFile(dir, "fruit.log", "orange\n");
  appendTempFile(dir, "animal.log", "axolotl\n");
  appendTempFile(dir, "color.log", "aquamarine\n");

  std::filesystem::rename(dir / "fruit.log", dir / "fruit.0");
  std::filesystem::rename(dir / "animal.log", dir / "animal.0");

  createTempFile(dir, "fruit.log", "peach\n");
  createTempFile(dir, "animal.log", "dinosaur\n");
  appendTempFile(dir, "color.log", "turquoise\n");

  plan->reset();
  LogTestController::getInstance().clear();

  TestController::runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 6 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.18-24.0"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:fruit.0-5.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.13-20.0"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:animal.0-8.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.23-33.log"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:color.34-43.log"));
}

TEST_CASE("TailFile handles the Rolling Filename Pattern property correctly", "[rotation]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto dir = testController.createTempDirectory();
  auto test_file = createTempFile(dir, "test.log", "some stuff\n", std::ios::out | std::ios::binary, -100ms);

  // Build MiNiFi processing graph
  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, test_file.string());

  std::vector<std::string> expected_log_lines;

  SECTION("If no pattern is set, we use the default, which is ${filename}.*, so the unrelated file will be picked up") {
    expected_log_lines = std::vector<std::string>{"Logged 2 flow files",
                                                  "test.rolled.11-24.log",
                                                  "test.0-15.txt"};
  }

  SECTION("If a pattern is set to exclude the unrelated file, we no longer pick it up") {
    plan->setProperty(tail_file, minifi::processors::TailFile::RollingFilenamePattern, "${filename}.*.log");

    expected_log_lines = std::vector<std::string>{"Logged 1 flow file",
                                                  "test.rolled.11-24.log"};
  }

  SECTION("We can also set the pattern to not include the file name") {
    plan->setProperty(tail_file, minifi::processors::TailFile::RollingFilenamePattern, "other_roll??.log");

    expected_log_lines = std::vector<std::string>{"Logged 1 flow file",
                                                  "other_rolled.11-24.log"};
  }

  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, minifi::processors::LogAttribute::FlowFilesToLog, "0");

  TestController::runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-10.log"));

  appendTempFile(dir, "test.log", "one more line\n");
  std::filesystem::rename(dir / "test.log", dir / "test.rolled.log");
  createTempFile(dir, "test.txt", "unrelated stuff\n");
  createTempFile(dir, "other_rolled.log", "some stuff\none more line\n");  // same contents as test.rolled.log

  plan->reset();
  LogTestController::getInstance().clear();

  TestController::runSession(plan, true);

  for (const auto &log_line : expected_log_lines) {
    REQUIRE(LogTestController::getInstance().contains(log_line));
  }
}

TEST_CASE("TailFile finds and finishes the renamed file and continues with the new log file after a restart", "[rotation][restart]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto log_dir = testController.createTempDirectory();

  const auto file_modi_time_t0 = std::chrono::file_clock::now();
  auto test_file_1 = createTempFile(log_dir, "test.1", "line one\nline two\nline three\n");  // old rotated file
  std::filesystem::last_write_time(log_dir/"test.1", file_modi_time_t0 - 1100ms);
  auto test_file = createTempFile(log_dir, "test.log", "line four\nline five\nline six\n");  // current log file
  std::filesystem::last_write_time(log_dir/"test.log", file_modi_time_t0 - 100ms);

  auto state_dir = testController.createTempDirectory();

  utils::Identifier tail_file_uuid = utils::IdGenerator::getIdGenerator()->generate();
  const core::Relationship success_relationship{"success", "everything is fine"};

  // use persistent state storage that defaults to rocksDB, not volatile
  const auto configuration = std::make_shared<minifi::ConfigureImpl>();
  configuration->setHome(log_dir);
  {
    auto test_plan = testController.createPlan(configuration, state_dir);
    auto tail_file = test_plan->addProcessor("TailFile", tail_file_uuid, "Tail", {success_relationship});
    test_plan->setProperty(tail_file, minifi::processors::TailFile::FileName, test_file.string());
    auto log_attr = test_plan->addProcessor("LogAttribute", "Log", success_relationship, true);
    test_plan->setProperty(log_attr, minifi::processors::LogAttribute::FlowFilesToLog, "0");
    test_plan->setProperty(log_attr, minifi::processors::LogAttribute::LogPayload, "true");

    TestController::runSession(test_plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  }

  LogTestController::getInstance().clear();

  appendTempFile(log_dir, "test.log", "line seven\n");
  std::filesystem::rename(log_dir / "test.1", log_dir / "test.2");
  std::filesystem::rename(log_dir / "test.log", log_dir / "test.1");
  createTempFile(log_dir, "test.log", "line eight is the last line\n");

  {
    auto test_plan = testController.createPlan(configuration, state_dir);
    auto tail_file = test_plan->addProcessor("TailFile", tail_file_uuid, "Tail", {success_relationship});
    test_plan->setProperty(tail_file, minifi::processors::TailFile::FileName, test_file.string());
    auto log_attr = test_plan->addProcessor("LogAttribute", "Log", success_relationship, true);
    test_plan->setProperty(log_attr, minifi::processors::LogAttribute::FlowFilesToLog, "0");
    test_plan->setProperty(log_attr, minifi::processors::LogAttribute::LogPayload, "true");

    TestController::runSession(test_plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:test.29-39.1"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-27.log"));
  }
}

TEST_CASE("TailFile yields if no work is done", "[yield]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto temp_directory = testController.createTempDirectory();
  auto plan = testController.createPlan();
  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tail_file, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, ".*\\.log");
  plan->setProperty(tail_file, minifi::processors::TailFile::BaseDirectory, temp_directory.string());
  plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "0 sec");

  SECTION("Empty log file => yield") {
    createTempFile(temp_directory, "first.log", "");

    TestController::runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() > 0ms);

    SECTION("No logging happened between onTrigger calls => yield") {
      plan->reset();
      tail_file->clearYield();

      TestController::runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() > 0ms);
    }

    SECTION("Some logging happened between onTrigger calls => don't yield") {
      plan->reset();
      tail_file->clearYield();

      appendTempFile(temp_directory, "first.log", "stuff stuff\nand stuff\n");

      TestController::runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() == 0ms);
    }
  }

  SECTION("Non-empty log file => don't yield") {
    createTempFile(temp_directory, "second.log", "some content\n");

    TestController::runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0ms);

    SECTION("No logging happened between onTrigger calls => yield") {
      plan->reset();
      tail_file->clearYield();

      TestController::runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() > 0ms);
    }

    SECTION("Some logging happened between onTrigger calls => don't yield") {
      plan->reset();
      tail_file->clearYield();

      appendTempFile(temp_directory, "second.log", "stuff stuff\nand stuff\n");

      TestController::runSession(plan, true);

      REQUIRE(tail_file->getYieldTime() == 0ms);
    }
  }
}

TEST_CASE("TailFile yields if no work is done on any files", "[yield][multiple_file]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  const auto temp_directory = testController.createTempDirectory();
  const auto plan = testController.createPlan();
  const auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tail_file, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, ".*\\.log");
  plan->setProperty(tail_file, minifi::processors::TailFile::BaseDirectory, temp_directory.string());
  plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "0 sec");

  createTempFile(temp_directory, "first.log", "stuff\n");
  createTempFile(temp_directory, "second.log", "different stuff\n");
  createTempFile(temp_directory, "third.log", "stuff stuff\n");

  TestController::runSession(plan, true);
  plan->reset();
  tail_file->clearYield();

  SECTION("No file changed => yield") {
    TestController::runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() > 0ms);
  }

  SECTION("One file changed => don't yield") {
    SECTION("first") { appendTempFile(temp_directory, "first.log", "more stuff\n"); }
    SECTION("second") { appendTempFile(temp_directory, "second.log", "more stuff\n"); }
    SECTION("third") { appendTempFile(temp_directory, "third.log", "more stuff\n"); }

    TestController::runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0ms);
  }

  SECTION("More than one file changed => don't yield") {
    SECTION("first and third") {
      appendTempFile(temp_directory, "first.log", "more stuff\n");
      appendTempFile(temp_directory, "third.log", "more stuff\n");
    }
    SECTION("all of them") {
      appendTempFile(temp_directory, "first.log", "more stuff\n");
      appendTempFile(temp_directory, "second.log", "more stuff\n");
      appendTempFile(temp_directory, "third.log", "more stuff\n");
    }

    TestController::runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0ms);
  }
}

TEST_CASE("TailFile doesn't yield if work was done on rotated files only", "[yield][rotation]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  const auto file_modi_time_t0 = std::chrono::file_clock::now();
  const auto temp_directory = testController.createTempDirectory();
  const auto full_file_name = createTempFile(temp_directory, "test.log", "stuff\n");
  std::filesystem::last_write_time(full_file_name, file_modi_time_t0 - 100ms);

  const auto plan = testController.createPlan();

  const auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, full_file_name.string());

  TestController::runSession(plan, true);

  plan->reset();
  tail_file->clearYield();

  SECTION("File rotated but not written => yield") {
    std::filesystem::rename(temp_directory / "test.log", temp_directory / "test.1");

    SECTION("Don't create empty new log file") {
    }
    SECTION("Create empty new log file") {
      createTempFile(temp_directory, "test.log", "");
    }

    TestController::runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() > 0ms);
  }

  SECTION("File rotated and new stuff is added => don't yield") {
    SECTION("New content before rotation") {
      appendTempFile(temp_directory, "test.log", "more stuff\n");
    }

    std::filesystem::rename(temp_directory / "test.log", temp_directory / "test.1");

    SECTION("New content after rotation") {
      createTempFile(temp_directory, "test.log", "even more stuff\n");
    }

    TestController::runSession(plan, true);

    REQUIRE(tail_file->getYieldTime() == 0ms);
  }
}

TEST_CASE("TailFile handles the Delimiter setting correctly", "[delimiter]") {
  std::vector<std::pair<std::string, std::string>> test_cases = {
      // first = value of Delimiter in the config
      // second = the expected delimiter char which will be used
      {"", ""}, {",", ","}, {"\t", "\t"}, {"\\t", "\t"}, {"\n", "\n"}, {"\\n", "\n"}, {"\\", "\\"}, {"\\\\", "\\"}};
  for (const auto &test_case : test_cases) {
    TestController testController;

    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
    LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

    auto temp_directory = testController.createTempDirectory();

    std::string delimiter = test_case.second;
    auto full_file_name = createTempFile(temp_directory, "test.log", utils::string::join_pack("one", delimiter, "two", delimiter));

    auto plan = testController.createPlan();

    auto tail_file = plan->addProcessor("TailFile", "Tail");
    plan->setProperty(tail_file, minifi::processors::TailFile::Delimiter, test_case.first);
    plan->setProperty(tail_file, minifi::processors::TailFile::FileName, full_file_name.string());

    auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
    plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

    TestController::runSession(plan, true);

    if (delimiter.empty()) {
      REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
      REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-5.log"));
    } else {
      REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
      REQUIRE(LogTestController::getInstance().contains("key:filename value:test.0-3.log"));
      REQUIRE(LogTestController::getInstance().contains("key:filename value:test.4-7.log"));
    }
  }
}

TEST_CASE("TailFile handles Unix/Windows line endings correctly", "[simple]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto temp_directory = testController.createTempDirectory();
  auto full_file_name = createTempFile(temp_directory, "test.log", "line1\nline two\n", std::ios::out);  // write in text mode

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, full_file_name.string());

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

  TestController::runSession(plan, true);

#ifdef WIN32
  std::size_t line_ending_size = 2;
#else
  std::size_t line_ending_size = 1;
#endif
  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(5 + line_ending_size) + " Offset:0"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(8 + line_ending_size) + " Offset:0"));
}

TEST_CASE("TailFile can tail all files in a directory recursively", "[multiple]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto base_directory = testController.createTempDirectory();
  auto directory1 = base_directory / "one";
  utils::file::FileUtils::create_dir(directory1);
  auto directory11 = directory1 / "one_child";
  utils::file::FileUtils::create_dir(directory11);
  auto directory2 = base_directory / "two";
  utils::file::FileUtils::create_dir(directory2);

  createTempFile(base_directory, "test.orange.log", "orange juice\n");
  createTempFile(directory1, "test.blue.log", "blue\n");
  createTempFile(directory1, "test.orange.log", "orange autumn leaves\n");
  createTempFile(directory11, "test.camel.log", "camel\n");
  createTempFile(directory2, "test.triangle.log", "triangle\n");

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, minifi::processors::TailFile::BaseDirectory, base_directory.string());
  plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "0 sec");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, ".*\\.log");

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");
  plan->setProperty(log_attribute, minifi::processors::LogAttribute::LogPayload, "true");

  SECTION("Recursive lookup not set => defaults to false") {
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  }

  SECTION("Recursive lookup set to false") {
    plan->setProperty(tail_file, minifi::processors::TailFile::RecursiveLookup, "false");
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow file"));
  }

  SECTION("Recursive lookup set to true") {
    plan->setProperty(tail_file, minifi::processors::TailFile::RecursiveLookup, "true");
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 5 flow files"));
  }
}

TEST_CASE("TailFile interprets the lookup frequency property correctly", "[multiple]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  auto directory = testController.createTempDirectory();
  createTempFile(directory, "test.red.log", "cherry\n");

  auto plan = testController.createPlan();

  auto tail_file = plan->addProcessor("TailFile", "Tail");
  plan->setProperty(tail_file, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, minifi::processors::TailFile::BaseDirectory, directory.string());
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, ".*\\.log");

  auto log_attribute = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

  SECTION("Lookup frequency not set => defaults to 10 minutes") {
    TestController::runSession(plan, true);

    const auto tail_file_processor = dynamic_cast<minifi::processors::TailFile*>(tail_file);
    REQUIRE(tail_file_processor);
    REQUIRE(tail_file_processor->getLookupFrequency() == std::chrono::minutes{10});
  }

  SECTION("Lookup frequency set to zero => new files are picked up immediately") {
    plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "0 sec");
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));

    plan->reset();
    LogTestController::getInstance().clear();

    createTempFile(directory, "test.blue.log", "sky\n");
    createTempFile(directory, "test.green.log", "grass\n");

    TestController::runSession(plan, true);

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  }

  SECTION("Lookup frequency set to 500 ms => new files are only picked up after 500 ms") {
    plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "500 ms");
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));

    plan->reset();
    LogTestController::getInstance().clear();

    createTempFile(directory, "test.blue.log", "sky\n");
    createTempFile(directory, "test.green.log", "grass\n");

    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 0 flow files"));

    plan->reset();
    LogTestController::getInstance().clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  }

  SECTION("Lookup frequency set to a thousand years => files already present when started are still picked up") {
    plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "365000 days");
    TestController::runSession(plan, true);
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  }
}

TEST_CASE("TailFile reads from a single file when Initial Start Position is set", "[initialStartPosition]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();
  createTempFile(dir, ROLLED_OVER_TMP_FILE, ROLLED_OVER_TAIL_DATA);
  auto temp_file_path = createTempFile(dir, TMP_FILE, NEWLINE_FILE);

  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  SECTION("Initial Start Position is set to Beginning of File") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Beginning of File");

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.find_first_of('\n') + 1) + " Offset:0"));

    plan->reset(true);
    LogTestController::getInstance().clear();

    appendTempFile(dir, TMP_FILE, NEW_TAIL_DATA);

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.size() - NEWLINE_FILE.find_first_of('\n') + NEW_TAIL_DATA.find_first_of('\n')) + " Offset:0"));
  }

  SECTION("Initial Start Position is set to Beginning of Time") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Beginning of Time");

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.find_first_of('\n') + 1) + " Offset:0"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(ROLLED_OVER_TAIL_DATA.find_first_of('\n') + 1) + " Offset:0"));

    plan->reset(true);
    LogTestController::getInstance().clear();

    appendTempFile(dir, TMP_FILE, NEW_TAIL_DATA);

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.size() - NEWLINE_FILE.find_first_of('\n') + NEW_TAIL_DATA.find_first_of('\n')) + " Offset:0"));
  }

  SECTION("Initial Start Position is set to Current Time") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Current Time");

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 0 flow files"));

    plan->reset(true);
    LogTestController::getInstance().clear();

    appendTempFile(dir, TMP_FILE, NEW_TAIL_DATA);

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEW_TAIL_DATA.find_first_of('\n') + 1) + " Offset:0"));
  }

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile reads from a single file when Initial Start Position is set to Current Time with rollover", "[initialStartPosition]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  const auto file_modi_time_t0 = std::chrono::file_clock::now();
  auto dir = testController.createTempDirectory();
  auto temp_file_path = createTempFile(dir, TMP_FILE, NEWLINE_FILE);
  std::filesystem::last_write_time(temp_file_path, file_modi_time_t0 - 100ms);

  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Current Time");

  TestController::runSession(plan);

  REQUIRE(LogTestController::getInstance().contains("Logged 0 flow files"));

  plan->reset(true);
  LogTestController::getInstance().clear();

  const std::string DATA_IN_NEW_FILE = "data in new file\n";
  appendTempFile(dir, TMP_FILE, NEW_TAIL_DATA);
  std::filesystem::rename(dir / TMP_FILE, dir / ROLLED_OVER_TMP_FILE);
  createTempFile(dir, TMP_FILE, DATA_IN_NEW_FILE);

  TestController::runSession(plan);

  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEW_TAIL_DATA.find_first_of('\n') + 1) + " Offset:0"));
  REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(DATA_IN_NEW_FILE.find_first_of('\n') + 1) + " Offset:0"));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile reads multiple files when Initial Start Position is set", "[initialStartPosition]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();
  createTempFile(dir, ROLLED_OVER_TMP_FILE, ROLLED_OVER_TAIL_DATA);
  createTempFile(dir, TMP_FILE, NEWLINE_FILE);
  const std::string TMP_FILE_2_DATA = "tmp_file_2_new_line_data\n";
  createTempFile(dir, "minifi-tmpfile-2.txt", TMP_FILE_2_DATA);

  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, ".*\\.txt");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::BaseDirectory, dir.string());

  SECTION("Initial Start Position is set to Beginning of File") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Beginning of File");

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.find_first_of('\n') + 1) + " Offset:0"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(TMP_FILE_2_DATA.find_first_of('\n') + 1) + " Offset:0"));

    plan->reset(true);
    LogTestController::getInstance().clear();

    createTempFile(dir, "minifi-tmpfile-3.txt", ADDITIONALY_CREATED_FILE_CONTENT);
    appendTempFile(dir, TMP_FILE, NEW_TAIL_DATA);

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(ADDITIONALY_CREATED_FILE_CONTENT.find_first_of('\n') + 1) + " Offset:0"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.size() - NEWLINE_FILE.find_first_of('\n') + NEW_TAIL_DATA.find_first_of('\n')) + " Offset:0"));
  }

  SECTION("Initial Start Position is set to Beginning of Time") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Beginning of Time");

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.find_first_of('\n') + 1) + " Offset:0"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(ROLLED_OVER_TAIL_DATA.find_first_of('\n') + 1) + " Offset:0"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(TMP_FILE_2_DATA.find_first_of('\n') + 1) + " Offset:0"));

    plan->reset(true);
    LogTestController::getInstance().clear();

    createTempFile(dir, "minifi-tmpfile-3.txt", ADDITIONALY_CREATED_FILE_CONTENT);
    appendTempFile(dir, TMP_FILE, NEW_TAIL_DATA);

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(ADDITIONALY_CREATED_FILE_CONTENT.find_first_of('\n') + 1) + " Offset:0"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEWLINE_FILE.size() - NEWLINE_FILE.find_first_of('\n') + NEW_TAIL_DATA.find_first_of('\n')) + " Offset:0"));
  }

  SECTION("Initial Start Position is set to Current Time") {
    plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Current Time");

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 0 flow files"));

    plan->reset(true);
    LogTestController::getInstance().clear();

    createTempFile(dir, "minifi-tmpfile-3.txt", ADDITIONALY_CREATED_FILE_CONTENT);
    appendTempFile(dir, TMP_FILE, NEW_TAIL_DATA);

    TestController::runSession(plan);

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(ADDITIONALY_CREATED_FILE_CONTENT.find_first_of('\n') + 1) + " Offset:0"));
    REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(NEW_TAIL_DATA.find_first_of('\n') + 1) + " Offset:0"));
  }

  LogTestController::getInstance().reset();
}

TEST_CASE("Initial Start Position is set to invalid or empty value", "[initialStartPosition]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tailfile = plan->addProcessor("TailFile", "tailfileProc");
  auto logattribute = plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  auto dir = testController.createTempDirectory();
  createTempFile(dir, ROLLED_OVER_TMP_FILE, ROLLED_OVER_TAIL_DATA);
  auto temp_file_path = createTempFile(dir, TMP_FILE, NEWLINE_FILE);

  plan->setProperty(logattribute, org::apache::nifi::minifi::processors::LogAttribute::FlowFilesToLog, "0");
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::FileName, temp_file_path.string());
  plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::Delimiter, "\n");

  CHECK_FALSE(plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, ""));
  CHECK_FALSE(plan->setProperty(tailfile, org::apache::nifi::minifi::processors::TailFile::InitialStartPosition, "Invalid Value"));
}

TEST_CASE("TailFile onSchedule throws if an invalid Attribute Provider Service is found", "[configuration][AttributeProviderService]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::TailFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto tail_file = plan->addProcessor("TailFile", "tailfileProc");
  plan->setProperty(tail_file, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, minifi::processors::TailFile::BaseDirectory, "/var/logs");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, "minifi.log");
  plan->setProperty(tail_file, org::apache::nifi::minifi::processors::TailFile::AttributeProviderService, "this AttributeProviderService does not exist");

  REQUIRE_THROWS_AS(plan->runNextProcessor(), minifi::Exception);
}

namespace {

class TestAttributeProviderService final : public minifi::controllers::AttributeProviderServiceImpl {
 public:
  using AttributeProviderServiceImpl::AttributeProviderServiceImpl;

  static constexpr const char* Description = "An attribute provider service which provides a constant set of records.";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override {};
  void onEnable() override {};
  std::optional<std::vector<AttributeMap>> getAttributes() override {
    return std::vector<AttributeMap>{AttributeMap{{"color", "red"}, {"fruit", "apple"}, {"uid", "001"}, {"animal", "dog"}},
                                     AttributeMap{{"color", "yellow"}, {"fruit", "banana"}, {"uid", "004"}, {"animal", "dolphin"}}};
  }
  std::string_view name() const override { return "test"; }
};
REGISTER_RESOURCE(TestAttributeProviderService, ControllerService);

}  // namespace

TEST_CASE("TailFile can use an AttributeProviderService", "[AttributeProviderService]") {
  TestController testController;
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();
  LogTestController::getInstance().setDebug<core::ProcessSession>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::filesystem::path temp_directory{testController.createTempDirectory()};

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  plan->addController("TestAttributeProviderService", "attribute_provider_service");
  auto tail_file = plan->addProcessor("TailFile", "tail_file");
  plan->setProperty(tail_file, minifi::processors::TailFile::TailMode, "Multiple file");
  plan->setProperty(tail_file, minifi::processors::TailFile::BaseDirectory, (temp_directory / "my_${color}_${fruit}_${uid}" / "${animal}").string());
  plan->setProperty(tail_file, minifi::processors::TailFile::LookupFrequency, "0 sec");
  plan->setProperty(tail_file, minifi::processors::TailFile::FileName, ".*\\.log");
  plan->setProperty(tail_file, minifi::processors::TailFile::AttributeProviderService, "attribute_provider_service");
  auto log_attribute = plan->addProcessor("LogAttribute", "log_attribute", core::Relationship("success", ""), true);
  plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

  createTempFile(temp_directory / "my_red_apple_001" / "dog", "0.log", "Idared\n");
  createTempFile(temp_directory / "my_red_apple_001" / "dog", "1.log", "Jonagold\n");
  createTempFile(temp_directory / "my_red_strawberry_002" / "elephant", "0.log", "red strawberry\n");
  createTempFile(temp_directory / "my_yellow_apple_003" / "horse", "0.log", "yellow apple\n");
  createTempFile(temp_directory / "my_yellow_banana_004" / "dolphin", "0.log", "yellow banana\n");

  TestController::runSession(plan);

  CHECK(LogTestController::getInstance().contains("Logged 3 flow files"));
  CHECK(LogTestController::getInstance().contains("key:absolute.path value:" + (temp_directory / "my_red_apple_001" / "dog" / "0.log").string()));
  CHECK(LogTestController::getInstance().contains("key:absolute.path value:" + (temp_directory / "my_red_apple_001" / "dog" / "1.log").string()));
  CHECK(LogTestController::getInstance().contains("key:absolute.path value:" + (temp_directory / "my_yellow_banana_004" / "dolphin" / "0.log").string()));

  CHECK(LogTestController::getInstance().contains("key:test.color value:red"));
  CHECK(LogTestController::getInstance().contains("key:test.color value:yellow"));
  CHECK(LogTestController::getInstance().contains("key:test.fruit value:apple"));
  CHECK(LogTestController::getInstance().contains("key:test.fruit value:banana"));
  CHECK(LogTestController::getInstance().contains("key:test.uid value:001"));
  CHECK(LogTestController::getInstance().contains("key:test.uid value:004"));
  CHECK(LogTestController::getInstance().contains("key:test.animal value:dog"));
  CHECK(LogTestController::getInstance().contains("key:test.animal value:dolphin"));

  CHECK_FALSE(LogTestController::getInstance().contains("key:test.fruit value:strawberry", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:test.uid value:002", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:test.uid value:003", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:test.animal value:elephant", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:test.animal value:horse", 0s, 0ms));

  LogTestController::getInstance().reset();
}

TEST_CASE("TailFile honors batch size for maximum lines processed", "[batchSize]") {
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();

  minifi::test::SingleProcessorTestController test_controller(std::make_unique<minifi::processors::TailFile>("TailFile"));
  auto tailfile = test_controller.getProcessor();

  auto temp_file_path  = test_controller.createTempDirectory() / TMP_FILE;

  std::ofstream tmpfile;
  tmpfile.open(temp_file_path, std::ios::out | std::ios::binary);
  for (auto i = 0; i < 20; ++i) {
    tmpfile << NEW_TAIL_DATA;
  }
  tmpfile.close();

  REQUIRE(tailfile->setProperty(minifi::processors::TailFile::FileName.name, temp_file_path.string()));
  REQUIRE(tailfile->setProperty(minifi::processors::TailFile::Delimiter.name, "\n"));
  REQUIRE(tailfile->setProperty(minifi::processors::TailFile::BatchSize.name, "10"));

  const auto result = test_controller.trigger();
  const auto& file_contents = result.at(minifi::processors::TailFile::Success);
  REQUIRE(file_contents.size() == 10);
}

TEST_CASE("Result mode tests") {
  LogTestController::getInstance().setTrace<minifi::processors::TailFile>();

  minifi::test::SingleProcessorTestController test_controller(std::make_unique<minifi::processors::TailFile>("TailFile"));
  auto tail_file = test_controller.getProcessor();

  auto temp_file_path  = test_controller.createTempDirectory() / TMP_FILE;

  const auto [result_mode, ff_count] = GENERATE(
    std::make_tuple(minifi::processors::TailResultFormat::FlowFilePerBatch, std::size_t{1}),
    std::make_tuple(minifi::processors::TailResultFormat::FlowFilePerDelimiter, std::size_t{5}));

  std::ofstream tmp_file;
  tmp_file.open(temp_file_path, std::ios::out | std::ios::binary);
  for (auto i = 0; i < 20; ++i) {
    tmp_file << NEW_TAIL_DATA;
  }
  tmp_file.close();

  CHECK(tail_file->setProperty(minifi::processors::TailFile::FileName.name, temp_file_path.string()));
  CHECK(tail_file->setProperty(minifi::processors::TailFile::Delimiter.name, "\n"));
  CHECK(tail_file->setProperty(minifi::processors::TailFile::BatchSize.name, "5"));
  CHECK(tail_file->setProperty(minifi::processors::TailFile::ResultFormat.name, std::string(magic_enum::enum_name(result_mode))));

  const auto result = test_controller.trigger();
  const auto& file_contents = result.at(minifi::processors::TailFile::Success);
  CHECK(file_contents.size() == ff_count);
}
