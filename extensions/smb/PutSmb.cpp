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

#include "PutSmb.hpp"
#include "SmbConnectionControllerService.hpp"
#include "core/PropertyBuilder.h"
#include "utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OsUtils.h"

namespace org::apache::nifi::minifi::extensions::smb {

const core::Property PutSmb::ConnectionControllerService(
    core::PropertyBuilder::createProperty("SMB Connection Controller Service")
      ->withDescription("Specifies the SMB connection controller service to use for connecting to the SMB server.")
      ->isRequired(true)
      ->asType<SmbConnectionControllerService>()
      ->build());

const core::Property PutSmb::Directory(
    core::PropertyBuilder::createProperty("Directory")
      ->withDescription(R"(The network folder to which files should be written. This is the remaining relative path after the share \\hostname\share\[dir1\dir2]. You may use expression language.)")
      ->isRequired(false)
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property PutSmb::CreateMissingDirectories(
    core::PropertyBuilder::createProperty("Create Missing Directories")
      ->withDescription("If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.")
      ->withDefaultValue(true)
      ->isRequired(true)
      ->build());

const core::Property PutSmb::ConflictResolution(
    core::PropertyBuilder::createProperty("Conflict Resolution Strategy")
      ->withDescription("Indicates what should happen when a file with the same name already exists in the output directory")
      ->withAllowableValues(FileExistsResolutionStrategy::values())
      ->withDefaultValue(toString(FileExistsResolutionStrategy::REPLACE_FILE))
      ->isRequired(true)
      ->build());


const core::Relationship PutSmb::Success("success", "Files that have been successfully written to the output network path are transferred to this relationship");
const core::Relationship PutSmb::Failure("failure", "Files that could not be written to the output network path for some reason are transferred to this relationship");

void PutSmb::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}


void PutSmb::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory*) {
  gsl_Expects(context);
  if (auto connection_controller_name = context->getProperty(PutSmb::ConnectionControllerService)) {
    smb_connection_controller_service_ = std::dynamic_pointer_cast<SmbConnectionControllerService>(context->getControllerService(*connection_controller_name));
  }
  if (!smb_connection_controller_service_) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing SMB Connection Controller Service");
  }

  create_missing_dirs_ = context->getProperty<bool>(PutSmb::CreateMissingDirectories).value_or(true);
  conflict_resolution_strategy_ = utils::parseEnumProperty<FileExistsResolutionStrategy>(*context, ConflictResolution);
}

std::filesystem::path PutSmb::getFilePath(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  auto filename = flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(flow_file->getUUIDStr());
  return smb_connection_controller_service_->getPath() / context.getProperty(Directory, flow_file).value_or("") / filename;
}

namespace {
class FlowFileToNetworkShareWriter {
 public:
  FlowFileToNetworkShareWriter(std::ofstream& file_stream) : file_stream_(file_stream) {}
  ~FlowFileToNetworkShareWriter() = default;
  int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
    size_t total_size_written = 0;
    std::array<std::byte, 1024> buffer{};

    do {
      size_t bytes_read = stream->read(buffer);
      if (io::isError(bytes_read))
        return -1;
      if (bytes_read == 0)
        break;
      file_stream_.write(reinterpret_cast<char *>(buffer.data()), gsl::narrow<std::streamsize>(bytes_read));
      total_size_written += bytes_read;
    } while (total_size_written < stream->size());

    return gsl::narrow<int64_t>(total_size_written);
  }

 private:
  std::ofstream& file_stream_;
};
}  // namespace

void PutSmb::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  gsl_Expects(context && session && smb_connection_controller_service_);

  if (!smb_connection_controller_service_->isConnected()) {
    auto connected = smb_connection_controller_service_->connect();
    if (!connected) {
      logger_->log_error("Couldn't establish connection to the specified network location");
      context->yield();
      return;
    }
  }

  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto full_file_path = getFilePath(*context, flow_file);


  if (!std::filesystem::exists(full_file_path.parent_path())) {
    if (create_missing_dirs_)
      std::filesystem::create_directories(full_file_path.parent_path());
    else {
      session->transfer(flow_file, Failure);
      return;
    }
  }


  if (std::filesystem::exists(full_file_path)) {
    if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::FAIL_FLOW) {
      session->transfer(flow_file, Failure);
      return;
    } else if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::IGNORE_REQUEST) {
      session->transfer(flow_file, Success);
      return;
    }
  }

  std::ofstream file_stream(full_file_path, std::ios::out | std::ios::trunc);
  if (!file_stream.is_open()) {
    logger_->log_error("Could not open file for writing");
    session->transfer(flow_file, Failure);
    return;
  }

  FlowFileToNetworkShareWriter flow_file_to_network_share_writer(file_stream);
  auto write_result = session->read(flow_file, flow_file_to_network_share_writer);
  if (io::isError(write_result)) {
    logger_->log_error("Failed to write flow file onto network share");
    session->transfer(flow_file, Failure);
    return;
  }

  session->transfer(flow_file, Success);
}

}  // namespace org::apache::nifi::minifi::extensions::smb
