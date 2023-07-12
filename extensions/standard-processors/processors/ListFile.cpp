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
#include "ListFile.h"

#include <filesystem>

#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Property ListFile::InputDirectory(
    core::PropertyBuilder::createProperty("Input Directory")
      ->withDescription("The input directory from which files to pull files")
      ->isRequired(true)
      ->build());

const core::Property ListFile::RecurseSubdirectories(
    core::PropertyBuilder::createProperty("Recurse Subdirectories")
      ->withDescription("Indicates whether to list files from subdirectories of the directory")
      ->withDefaultValue(true)
      ->isRequired(true)
      ->build());

const core::Property ListFile::FileFilter(
    core::PropertyBuilder::createProperty("File Filter")
      ->withDescription("Only files whose names match the given regular expression will be picked up")
      ->build());

const core::Property ListFile::PathFilter(
    core::PropertyBuilder::createProperty("Path Filter")
      ->withDescription("When Recurse Subdirectories is true, then only subdirectories whose path matches the given regular expression will be scanned")
      ->build());

const core::Property ListFile::MinimumFileAge(
    core::PropertyBuilder::createProperty("Minimum File Age")
      ->withDescription("The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored")
      ->isRequired(true)
      ->withDefaultValue<core::TimePeriodValue>("0 sec")
      ->build());

const core::Property ListFile::MaximumFileAge(
    core::PropertyBuilder::createProperty("Maximum File Age")
      ->withDescription("The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored")
      ->build());

const core::Property ListFile::MinimumFileSize(
    core::PropertyBuilder::createProperty("Minimum File Size")
      ->withDescription("The minimum size that a file must be in order to be pulled")
      ->isRequired(true)
      ->withDefaultValue<core::DataSizeValue>("0 B")
      ->build());

const core::Property ListFile::MaximumFileSize(
    core::PropertyBuilder::createProperty("Maximum File Size")
      ->withDescription("The maximum size that a file can be in order to be pulled")
      ->build());

const core::Property ListFile::IgnoreHiddenFiles(
    core::PropertyBuilder::createProperty("Ignore Hidden Files")
      ->withDescription("Indicates whether or not hidden files should be ignored")
      ->withDefaultValue(true)
      ->isRequired(true)
      ->build());

const core::Relationship ListFile::Success("success", "All FlowFiles that are received are routed to success");

const core::OutputAttribute ListFile::Filename{"filename", { Success }, "The name of the file that was read from filesystem."};
const core::OutputAttribute ListFile::Path{"path", { Success },
    "The path is set to the relative path of the file's directory on filesystem compared to the Input Directory property. "
    "For example, if Input Directory is set to /tmp, then files picked up from /tmp will have the path attribute set to \"./\". "
    "If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to \"abc/1/2/3/\"."};
const core::OutputAttribute ListFile::AbsolutePath{"absolute.path", { Success },
    "The absolute.path is set to the absolute path of the file's directory on filesystem. "
    "For example, if the Input Directory property is set to /tmp, then files picked up from /tmp will have the path attribute set to \"/tmp/\". "
    "If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to \"/tmp/abc/1/2/3/\"."};
const core::OutputAttribute ListFile::FileOwner{"file.owner", { Success }, "The user that owns the file in filesystem"};
const core::OutputAttribute ListFile::FileGroup{"file.group", { Success }, "The group that owns the file in filesystem"};
const core::OutputAttribute ListFile::FileSize{"file.size", { Success }, "The number of bytes in the file in filesystem"};
const core::OutputAttribute ListFile::FilePermissions{"file.permissions", { Success },
    "The permissions for the file in filesystem. This is formatted as 3 characters for the owner, 3 for the group, and 3 for other users. For example rw-rw-r--"};
const core::OutputAttribute ListFile::FileLastModifiedTime{"file.lastModifiedTime", { Success }, "The timestamp of when the file in filesystem was last modified as 'yyyy-MM-dd'T'HH:mm:ssZ'"};

void ListFile::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ListFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &/*sessionFactory*/) {
  gsl_Expects(context);

  auto state_manager = context->getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  if (auto input_directory_str = context->getProperty(InputDirectory); !input_directory_str || input_directory_str->empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory property missing or invalid");
  } else {
    input_directory_ = *input_directory_str;
  }

  context->getProperty(RecurseSubdirectories.getName(), recurse_subdirectories_);

  std::string value;
  if (context->getProperty(FileFilter.getName(), value) && !value.empty()) {
    file_filter_.filename_filter = std::regex(value);
  }

  if (recurse_subdirectories_ && context->getProperty(PathFilter.getName(), value) && !value.empty()) {
    file_filter_.path_filter = std::regex(value);
  }

  if (auto minimum_file_age = context->getProperty<core::TimePeriodValue>(MinimumFileAge)) {
    file_filter_.minimum_file_age =  minimum_file_age->getMilliseconds();
  }

  if (auto maximum_file_age = context->getProperty<core::TimePeriodValue>(MaximumFileAge)) {
    file_filter_.maximum_file_age =  maximum_file_age->getMilliseconds();
  }

  uint64_t int_value = 0;
  if (context->getProperty(MinimumFileSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, int_value)) {
    file_filter_.minimum_file_size = int_value;
  }

  if (context->getProperty(MaximumFileSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, int_value)) {
    file_filter_.maximum_file_size = int_value;
  }

  context->getProperty(IgnoreHiddenFiles.getName(), file_filter_.ignore_hidden_files);
}

std::shared_ptr<core::FlowFile> ListFile::createFlowFile(core::ProcessSession& session, const utils::ListedFile& listed_file) {
  auto flow_file = session.create();
  session.putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, listed_file.getPath().filename().string());
  session.putAttribute(flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, (listed_file.getPath().parent_path() / "").string());

  auto relative_path = std::filesystem::relative(listed_file.getPath().parent_path(), input_directory_);
  session.putAttribute(flow_file, core::SpecialFlowAttribute::PATH, (relative_path / "").string());

  session.putAttribute(flow_file, ListFile::FileSize.getName(), std::to_string(utils::file::file_size(listed_file.getPath())));
  session.putAttribute(flow_file, ListFile::FileLastModifiedTime.getName(), utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(listed_file.getLastModified())));

  if (auto permission_string = utils::file::FileUtils::get_permission_string(listed_file.getPath())) {
    session.putAttribute(flow_file, ListFile::FilePermissions.getName(), *permission_string);
  } else {
    logger_->log_warn("Failed to get permissions of file '%s'", listed_file.getPath().string());
    session.putAttribute(flow_file, ListFile::FilePermissions.getName(), "");
  }

  if (auto owner = utils::file::FileUtils::get_file_owner(listed_file.getPath())) {
    session.putAttribute(flow_file, ListFile::FileOwner.getName(), *owner);
  } else {
    logger_->log_warn("Failed to get owner of file '%s'", listed_file.getPath().string());
    session.putAttribute(flow_file, ListFile::FileOwner.getName(), "");
  }

#ifndef WIN32
  if (auto group = utils::file::FileUtils::get_file_group(listed_file.getPath())) {
    session.putAttribute(flow_file, ListFile::FileGroup.getName(), *group);
  } else {
    logger_->log_warn("Failed to get group of file '%s'", listed_file.getPath().string());
    session.putAttribute(flow_file, ListFile::FileGroup.getName(), "");
  }
#else
  session.putAttribute(flow_file, ListFile::FileGroup.getName(), "");
#endif

  return flow_file;
}

void ListFile::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session);

  auto stored_listing_state = state_manager_->getCurrentState();
  auto latest_listing_state = stored_listing_state;
  uint32_t files_listed = 0;

  auto process_files = [&](const std::filesystem::path& path, const std::filesystem::path& filename) {
    auto listed_file = utils::ListedFile(path / filename);

    if (stored_listing_state.wasObjectListedAlready(listed_file) || !listed_file.matches(file_filter_)) {
      return true;
    }

    session->transfer(createFlowFile(*session, listed_file), Success);
    ++files_listed;
    latest_listing_state.updateState(listed_file);
    return true;
  };
  utils::file::list_dir(input_directory_, process_files, logger_, recurse_subdirectories_);

  state_manager_->storeState(latest_listing_state);

  if (files_listed == 0) {
    logger_->log_debug("No new files were found in input directory '%s' to list", input_directory_.string());
    context->yield();
  }
}

REGISTER_RESOURCE(ListFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
