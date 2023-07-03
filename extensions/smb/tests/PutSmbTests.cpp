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

#include "TestBase.h"
#include "Catch.h"
#include "PutSmb.h"
#include "SmbConnectionControllerService.h"
#include "SmbTestUtils.h"
#include "SingleProcessorTestController.h"
#include "range/v3/algorithm/count_if.hpp"

namespace org::apache::nifi::minifi::extensions::smb::test {

std::string checkFileContent(const std::filesystem::path& path) {
  gsl_Expects(std::filesystem::exists(path));
  std::ifstream if_stream(path);
  return {std::istreambuf_iterator<char>(if_stream), std::istreambuf_iterator<char>()};
}

TEST_CASE("PutSmb invalid network path") {
  const auto put_smb = std::make_shared<PutSmb>("PutSmb");
  minifi::test::SingleProcessorTestController controller{put_smb};

  auto smb_connection_node = controller.plan->addController("SmbConnectionControllerService", "smb_connection_controller_service");

  controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Hostname.getName(), utils::OsUtils::getHostName().value_or("localhost"));
  controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Share.getName(), "some_share_that_does_not_exists");
  controller.plan->setProperty(put_smb, PutSmb::ConnectionControllerService.getName(), "smb_connection_controller_service");

  controller.plan->setProperty(put_smb, PutSmb::ConflictResolution.getName(), toString(PutSmb::FileExistsResolutionStrategy::IGNORE_REQUEST));

  std::string file_name = "my_file.txt";

  const auto first_trigger_results = controller.trigger("alpha", {{core::SpecialFlowAttribute::FILENAME, file_name}});

  CHECK(first_trigger_results.at(PutSmb::Failure).empty());
  CHECK(first_trigger_results.at(PutSmb::Success).empty());

  CHECK(put_smb->isYield());
}

TEST_CASE("PutSmb conflict resolution test") {
  const auto put_smb = std::make_shared<PutSmb>("PutSmb");
  minifi::test::SingleProcessorTestController controller{put_smb};

  auto temp_directory = controller.createTempDirectory();
  auto share_name = temp_directory.filename().wstring();
  auto temp_smb_share = TempSmbShare::create(share_name, temp_directory.wstring());
  REQUIRE(temp_smb_share);

  auto smb_connection_node = controller.plan->addController("SmbConnectionControllerService", "smb_connection_controller_service");

  controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Hostname.getName(), utils::OsUtils::getHostName().value_or("localhost"));
  controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Share.getName(), minifi::utils::OsUtils::wideStringToString(share_name));
  controller.plan->setProperty(put_smb, PutSmb::ConnectionControllerService.getName(), "smb_connection_controller_service");

  SECTION("Replace") {
    controller.plan->setProperty(put_smb, PutSmb::ConflictResolution.getName(), toString(PutSmb::FileExistsResolutionStrategy::REPLACE_FILE));

    std::string file_name = "my_file.txt";

    CHECK_FALSE(std::filesystem::exists(temp_directory / file_name));

    const auto first_trigger_results = controller.trigger("alpha", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");

    const auto second_trigger_results = controller.trigger("beta", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(second_trigger_results.at(PutSmb::Failure).empty());
    CHECK(second_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "beta");
  }

  SECTION("Ignore") {
    controller.plan->setProperty(put_smb, PutSmb::ConflictResolution.getName(), toString(PutSmb::FileExistsResolutionStrategy::IGNORE_REQUEST));

    std::string file_name = "my_file.txt";

    CHECK_FALSE(std::filesystem::exists(temp_directory / file_name));

    const auto first_trigger_results = controller.trigger("alpha", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");

    const auto second_trigger_results = controller.trigger("beta", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(second_trigger_results.at(PutSmb::Failure).empty());
    CHECK(second_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");
  }

  SECTION("Fail") {
    controller.plan->setProperty(put_smb, PutSmb::ConflictResolution.getName(), toString(PutSmb::FileExistsResolutionStrategy::FAIL_FLOW));

    std::string file_name = "my_file.txt";

    CHECK_FALSE(std::filesystem::exists(temp_directory / file_name));

    const auto first_trigger_results = controller.trigger("alpha", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");

    const auto second_trigger_results = controller.trigger("beta", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(second_trigger_results.at(PutSmb::Failure).size() == 1);
    CHECK(second_trigger_results.at(PutSmb::Success).empty());

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");
  }
}

TEST_CASE("PutSmb create missing dirs test") {
  const auto put_smb = std::make_shared<PutSmb>("PutSmb");
  minifi::test::SingleProcessorTestController controller{put_smb};

  auto temp_directory = controller.createTempDirectory();
  auto share_name = temp_directory.filename().wstring();
  auto temp_smb_share = TempSmbShare::create(share_name, temp_directory.wstring());
  REQUIRE(temp_smb_share);

  auto smb_connection_node = controller.plan->addController("SmbConnectionControllerService", "smb_connection_controller_service");

  controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Hostname.getName(), "localhost");
  controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Share.getName(), minifi::utils::OsUtils::wideStringToString(share_name));
  controller.plan->setProperty(put_smb, PutSmb::ConnectionControllerService.getName(), "smb_connection_controller_service");
  controller.plan->setProperty(put_smb, PutSmb::Directory.getName(), "a/b");

  SECTION("Create missing dirs") {
    controller.plan->setProperty(put_smb, PutSmb::CreateMissingDirectories.getName(), "true");
    std::string file_name = "my_file.txt";

    auto expected_path = temp_directory / "a" / "b" / file_name;

    CHECK_FALSE(std::filesystem::exists(expected_path));

    const auto first_trigger_results = controller.trigger("alpha", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    REQUIRE(std::filesystem::exists(expected_path));
    CHECK(checkFileContent(expected_path) == "alpha");
  }

  SECTION("Don't create missing dirs") {
    controller.plan->setProperty(put_smb, PutSmb::CreateMissingDirectories.getName(), "false");
    std::string file_name = "my_file.txt";

    auto expected_path = temp_directory / "a" / "b" / file_name;

    CHECK_FALSE(std::filesystem::exists(expected_path));

    const auto first_trigger_results = controller.trigger("alpha", {{core::SpecialFlowAttribute::FILENAME, file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).size() == 1);
    CHECK(first_trigger_results.at(PutSmb::Success).empty());

    CHECK_FALSE(std::filesystem::exists(expected_path));
  }
}



}  // namespace org::apache::nifi::minifi::extensions::smb::test
