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
#include "SmbConnectionControllerService.hpp"
#include "SmbTestUtils.hpp"

namespace org::apache::nifi::minifi::extensions::smb::test {

struct SmbConnectionControllerServiceFixture {
  SmbConnectionControllerServiceFixture() = default;

  TestController test_controller_{};
  std::shared_ptr<TestPlan> plan_ = test_controller_.createPlan();
  std::shared_ptr<minifi::core::controller::ControllerServiceNode>  smb_connection_node_ = plan_->addController("SmbConnectionControllerService", "smb_connection_controller_service");
  std::shared_ptr<SmbConnectionControllerService> smb_connection_ = std::dynamic_pointer_cast<SmbConnectionControllerService>(smb_connection_node_->getControllerServiceImplementation());
};


TEST_CASE_METHOD(SmbConnectionControllerServiceFixture, "SmbConnectionControllerService onEnable throws when empty") {
  REQUIRE_THROWS(plan_->finalize());
}

TEST_CASE_METHOD(SmbConnectionControllerServiceFixture, "SmbConnectionControllerService anonymous connection") {
  auto temp_directory = test_controller_.createTempDirectory();
  auto share_local_name = temp_directory.filename().wstring();

  auto temp_smb_share = TempSmbShare::create(share_local_name, temp_directory.wstring());
  REQUIRE(temp_smb_share);

  SECTION("Valid share") {
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Hostname.getName(), "localhost");
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Share.getName(), minifi::utils::OsUtils::wideStringToString(share_local_name));

    REQUIRE_NOTHROW(plan_->finalize());

    auto smb_connection_status = smb_connection_->isConnected();
    REQUIRE(smb_connection_status);
    CHECK_FALSE(*smb_connection_status);

    auto connection_result = smb_connection_->connect();
    REQUIRE(connection_result);

    smb_connection_status = smb_connection_->isConnected();
    REQUIRE(smb_connection_status);
    CHECK(*smb_connection_status);
  }

  SECTION("Invalid share") {
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Hostname.getName(), "localhost");
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Share.getName(), "invalid_share_name");

    REQUIRE_NOTHROW(plan_->finalize());

    auto smb_connection_status = smb_connection_->isConnected();
    REQUIRE(smb_connection_status);
    CHECK_FALSE(*smb_connection_status);

    auto connection_result = smb_connection_->connect();
    REQUIRE_FALSE(connection_result);

    smb_connection_status = smb_connection_->isConnected();
    REQUIRE(smb_connection_status);
    CHECK_FALSE(*smb_connection_status);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::smb::test
