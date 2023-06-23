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
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::extensions::smb {

class PutSmbFile : public core::Processor {
 public:
  explicit PutSmbFile(std::string name,  const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  ~PutSmbFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Writes the contents of a FlowFile to an smb network location";

  EXTENSIONAPI static const core::Property Hostname;
  EXTENSIONAPI static const core::Property Share;
  EXTENSIONAPI static const core::Property Directory;
  EXTENSIONAPI static const core::Property Domain;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;
  EXTENSIONAPI static const core::Property CreateMissingDirectories;
  EXTENSIONAPI static const core::Property ShareAccessStrategy;
  EXTENSIONAPI static const core::Property ConflictResolution;
  EXTENSIONAPI static const core::Property BatchSize;
  EXTENSIONAPI static const core::Property TemporarySuffix;
  EXTENSIONAPI static const core::Property SmbDialect;
  EXTENSIONAPI static const core::Property UseEncryption;

  SMART_ENUM(ShareAccessStrategies,
    (kNone, "none"),
    (kRead, "read"),
    (kReadDelete, "read, delete"),
    (kReadWriteDelete, "read, write, delete")
  )

  SMART_ENUM(ConflictResolutionStrategies,
    (kReplace, "replace"),
    (kIgnore, "ignore"),
    (kFail, "fail")
  )

  SMART_ENUM(SmbDialects,
    (kAuto, "AUTO"),
    (kSmb2_0_2, "SMB 2.0.2"),
    (kSmb2_1, "SMB 2.1"),
    (kSmb3_0, "SMB 3.0"),
    (kSmb3_0_2, "SMB 3.0.2"),
    (kSmb3_1_1, "SMB 3.1.1")
  )

  static auto properties() {
    return std::array{
      Hostname,
      Share,
      Directory,
      Domain,
      Username,
      Password,
      CreateMissingDirectories,
      ShareAccessStrategy,
      ConflictResolution,
      BatchSize,
      TemporarySuffix,
      SmbDialect,
      UseEncryption
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutSmbFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::extensions::smb
