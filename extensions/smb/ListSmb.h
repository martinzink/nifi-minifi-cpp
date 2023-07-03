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

#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>

#include "SmbConnectionControllerService.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "utils/ListingStateManager.h"
#include "utils/file/ListedFile.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::extensions::smb {

class ListSmb : public core::Processor {
 public:
  explicit ListSmb(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Retrieves a listing of files from an SMB share. For each file that is listed, "
                                                          "creates a FlowFile that represents the file so that it can be fetched in conjunction with FetchSmb.";

  EXTENSIONAPI static const core::Property ConnectionControllerService;
  EXTENSIONAPI static const core::Property InputDirectory;
  EXTENSIONAPI static const core::Property RecurseSubdirectories;
  EXTENSIONAPI static const core::Property FileFilter;
  EXTENSIONAPI static const core::Property PathFilter;
  EXTENSIONAPI static const core::Property MinimumFileAge;
  EXTENSIONAPI static const core::Property MaximumFileAge;
  EXTENSIONAPI static const core::Property MinimumFileSize;
  EXTENSIONAPI static const core::Property MaximumFileSize;
  EXTENSIONAPI static const core::Property IgnoreHiddenFiles;
  static auto properties() {
    return std::array{
        InputDirectory,
        RecurseSubdirectories,
        FileFilter,
        PathFilter,
        MinimumFileAge,
        MaximumFileAge,
        MinimumFileSize,
        MaximumFileSize,
        IgnoreHiddenFiles
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static const core::OutputAttribute Filename;
  EXTENSIONAPI static const core::OutputAttribute Path;
  EXTENSIONAPI static const core::OutputAttribute AbsolutePath;
  EXTENSIONAPI static const core::OutputAttribute FileOwner;
  EXTENSIONAPI static const core::OutputAttribute FileGroup;
  EXTENSIONAPI static const core::OutputAttribute FileSize;
  EXTENSIONAPI static const core::OutputAttribute FilePermissions;
  EXTENSIONAPI static const core::OutputAttribute FileLastModifiedTime;
  static auto outputAttributes() {
    return std::array{
        Filename,
        Path,
        AbsolutePath,
        FileOwner,
        FileGroup,
        FileSize,
        FilePermissions,
        FileLastModifiedTime
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  bool fileMatchesFilters(const utils::ListedFile& listed_file);
  std::shared_ptr<core::FlowFile> createFlowFile(core::ProcessSession& session, const utils::ListedFile& listed_file);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListSmb>::getLogger(uuid_);
  std::filesystem::path input_directory_;
  std::shared_ptr<SmbConnectionControllerService> smb_connection_controller_service_;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
  bool recurse_subdirectories_ = true;
  utils::FileFilter file_filter_{};
};

}  // namespace org::apache::nifi::minifi::extensions::smb
