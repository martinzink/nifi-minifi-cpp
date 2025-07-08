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
#pragma once

#include "minifi-cpp/properties/Locations.h"
#include "Defaults.h"

namespace org::apache::nifi::minifi {
class LocationsImpl : public Locations {
public:
  explicit LocationsImpl(const std::filesystem::path& minifi_home) {
    working_dir_ = minifi_home;
    lock_path_ = minifi_home / "LOCK";
    log_properties_path_ = minifi_home / DEFAULT_LOG_PROPERTIES_FILE;
    uid_properties_path_ = minifi_home / DEFAULT_UID_PROPERTIES_FILE;
    properties_path_ = minifi_home / DEFAULT_NIFI_PROPERTIES_FILE;
    log_path_ = minifi_home / "logs";
  }

  std::filesystem::path getWorkingDir() const override { return working_dir_; }
  std::filesystem::path getLockPath() const override { return lock_path_; }
  std::filesystem::path getLogPropertiesPath() const override { return log_properties_path_; }
  std::filesystem::path getUidPropertiesPath() const override { return uid_properties_path_; }
  std::filesystem::path getPropertiesPath() const override { return properties_path_; }

private:
  std::filesystem::path working_dir_;
  std::filesystem::path lock_path_;
  std::filesystem::path log_properties_path_;
  std::filesystem::path uid_properties_path_;
  std::filesystem::path properties_path_;
  std::filesystem::path log_path_;
};
}  // namespace org::apache::nifi::minifi
