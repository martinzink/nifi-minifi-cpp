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

#include "ListSmb.h"
#include <filesystem>

  #include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::smb {

const core::Property ListSmb::ConnectionControllerService(
    core::PropertyBuilder::createProperty("SMB Connection Controller Service")
        ->withDescription("Specifies the SMB connection controller service to use for connecting to the SMB server.")
        ->isRequired(true)
        ->asType<SmbConnectionControllerService>()
        ->build());


const core::Property ListSmb::InputDirectory(
    core::PropertyBuilder::createProperty("Input Directory")
        ->withDescription("The input directory from which files to pull files")
        ->isRequired(true)
        ->build());

const core::Property ListSmb::RecurseSubdirectories(
    core::PropertyBuilder::createProperty("Recurse Subdirectories")
        ->withDescription("Indicates whether to list files from subdirectories of the directory")
        ->withDefaultValue(true)
        ->isRequired(true)
        ->build());

const core::Property ListSmb::FileFilter(
    core::PropertyBuilder::createProperty("File Filter")
        ->withDescription("Only files whose names match the given regular expression will be picked up")
        ->build());

const core::Property ListSmb::PathFilter(
    core::PropertyBuilder::createProperty("Path Filter")
        ->withDescription("When Recurse Subdirectories is true, then only subdirectories whose path matches the given regular expression will be scanned")
        ->build());

const core::Property ListSmb::MinimumFileAge(
    core::PropertyBuilder::createProperty("Minimum File Age")
        ->withDescription("The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored")
        ->isRequired(true)
        ->withDefaultValue<core::TimePeriodValue>("0 sec")
        ->build());

const core::Property ListSmb::MaximumFileAge(
    core::PropertyBuilder::createProperty("Maximum File Age")
        ->withDescription("The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored")
        ->build());

const core::Property ListSmb::MinimumFileSize(
    core::PropertyBuilder::createProperty("Minimum File Size")
        ->withDescription("The minimum size that a file must be in order to be pulled")
        ->isRequired(true)
        ->withDefaultValue<core::DataSizeValue>("0 B")
        ->build());

const core::Property ListSmb::MaximumFileSize(
    core::PropertyBuilder::createProperty("Maximum File Size")
        ->withDescription("The maximum size that a file can be in order to be pulled")
        ->build());

const core::Property ListSmb::IgnoreHiddenFiles(
    core::PropertyBuilder::createProperty("Ignore Hidden Files")
        ->withDescription("Indicates whether or not hidden files should be ignored")
        ->withDefaultValue(true)
        ->isRequired(true)
        ->build());

const core::Relationship ListSmb::Success("success", "All FlowFiles that are received are routed to success");

const core::OutputAttribute ListSmb::Filename{"filename", { Success }, "The name of the file that was read from filesystem."};
const core::OutputAttribute ListSmb::ShortName{"shortName", { Success }, "The name of the file that was read from filesystem."};
const core::OutputAttribute ListSmb::Path{"path", { Success },
    "The path is set to the relative path of the file's directory on the remote filesystem compared to the Share root directory. "
    "For example, for a given remote locationsmb://HOSTNAME:PORT/SHARE/DIRECTORY, and a file is being listed from smb://HOSTNAME:PORT/SHARE/DIRECTORY/sub/folder/file "
    "then the path attribute will be set to \"DIRECTORY/sub/folder\"."};
const core::OutputAttribute ListSmb::ServiceLocation	{"serviceLocation", { Success },
    "The SMB URL of the share."};
const core::OutputAttribute ListSmb::LastModifiedTime	{"lastModifiedTime", { Success },
    "The timestamp of when the file's content changed in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'."};
const core::OutputAttribute ListSmb::CreationTime	{"creationTime", { Success },
    "The timestamp of when the file was created in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'."};
const core::OutputAttribute ListSmb::LastAccessTime	{"lastAccessTime", { Success },
    "The timestamp of when the file was accessed in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'."};
const core::OutputAttribute ListSmb::ChangeTime{"changeTime", { Success },
    "The timestamp of when the file's attributes was changed in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'."};

const core::OutputAttribute ListSmb::Size{"size", { Success }, "The size of the file in bytes.."};
const core::OutputAttribute ListSmb::AllocationSize{"allocationSize", { Success }, "The number of bytes allocated for the file on the server."};

void ListSmb::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ListSmb::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &/*sessionFactory*/) {
  gsl_Expects(context);

  if (auto connection_controller_name = context->getProperty(ListSmb::ConnectionControllerService)) {
    smb_connection_controller_service_ = std::dynamic_pointer_cast<SmbConnectionControllerService>(context->getControllerService(*connection_controller_name));
  }
  if (!smb_connection_controller_service_) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing SMB Connection Controller Service");
  }

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

std::shared_ptr<core::FlowFile> ListSmb::createFlowFile(core::ProcessSession& session, const utils::ListedFile& listed_file) {
  auto flow_file = session.create();
  session.putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, listed_file.getPath().filename().string());
  session.putAttribute(flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, (listed_file.getPath().parent_path() / "").string());

  auto relative_path = std::filesystem::relative(listed_file.getPath().parent_path(), input_directory_);
  session.putAttribute(flow_file, core::SpecialFlowAttribute::PATH, (relative_path / "").string());

  session.putAttribute(flow_file, ListSmb::Filename.getName(), std::to_string(utils::file::file_size(listed_file.getPath())));
  session.putAttribute(flow_file, "file.lastModifiedTime", utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(listed_file.getLastModified())));

  return flow_file;
}

void ListSmb::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session && smb_connection_controller_service_);

  auto connection_error = smb_connection_controller_service_->validateConnection();
  if (connection_error) {
    logger_->log_error("Couldn't establish connection to the specified network location due to %s", connection_error.message());
    context->yield();
    return;
  }

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

REGISTER_RESOURCE(ListSmb, Processor);

}  // namespace org::apache::nifi::minifi::extensions::smb
