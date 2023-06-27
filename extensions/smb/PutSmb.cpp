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

const core::Property PutSmb::ShareAccessStrategy(
    core::PropertyBuilder::createProperty("Share Access Strategy")
      ->withDescription("Indicates which shared access are granted on the file during the write. None is the most restrictive, but the safest setting to prevent corruption.")
      ->withAllowableValues(ShareAccessStrategies::values())
      ->withDefaultValue(toString(ShareAccessStrategies::kNone))
      ->isRequired(true)
      ->build());

const core::Property PutSmb::ConflictResolution(
    core::PropertyBuilder::createProperty("Conflict Resolution Strategy")
      ->withDescription("Indicates what should happen when a file with the same name already exists in the output directory")
      ->withAllowableValues(FileExistsResolutionStrategy::values())
      ->withDefaultValue(toString(FileExistsResolutionStrategy::REPLACE_FILE))
      ->isRequired(true)
      ->build());

const core::Property PutSmb::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")
    ->withDescription("The maximum number of files to put in each iteration")
    ->withDefaultValue<uint64_t>(100)
    ->isRequired(true)
    ->build());


const core::Property PutSmb::TemporarySuffix(
    core::PropertyBuilder::createProperty("Temporary Suffix")
      ->withDescription("A temporary suffix which will be appended to the filename while it's transferring. After the transfer is complete, the suffix will be removed.")
      ->isRequired(false)
      ->build());

const core::Property PutSmb::UseEncryption(
    core::PropertyBuilder::createProperty("Use Encryption")
      ->withDescription("Turns on/off encrypted communication between the client and the server. "
                        "The property's behavior is SMB dialect dependent: SMB 2.x does not support encryption and the property has no effect. "
                        "In case of SMB 3.x, it is a hint/request to the server to turn encryption on if the server also supports it.")
      ->withDefaultValue(false)
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

std::string PutSmb::getFilePath(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  std::vector<std::string> path_parts;
  path_parts.push_back(smb_connection_controller_service_->getPath());
  if (auto directory = context.getProperty(Directory, flow_file)) {
    path_parts.push_back(*directory);
  }
  path_parts.push_back(flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(flow_file->getUUIDStr()));
  return minifi::utils::StringUtils::join("\\", path_parts);
}

namespace {
class FlowFileToNetworkShareWriter {
 public:
  FlowFileToNetworkShareWriter(HANDLE file_handle) : file_handle_(file_handle) {}
  ~FlowFileToNetworkShareWriter() = default;
  int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
    write_succeeded_ = false;
    size_t total_size_written = 0;
    std::array<std::byte, 1024> buffer{};

    do {
      DWORD bytes_read = gsl::narrow<DWORD>(stream->read(buffer));
      if (io::isError(bytes_read))
        return -1;
      if (bytes_read == 0)
        break
        ;
      DWORD bytes_written;
      bool write_success = WriteFile(file_handle_, buffer.data(), bytes_read, &bytes_written, NULL);
      if (!write_success || bytes_read != bytes_written)
        return -1;
      total_size_written += bytes_written;
    } while (total_size_written < stream->size());

    write_succeeded_ = true;

    return gsl::narrow<int64_t>(total_size_written);
  }

 private:
  bool write_succeeded_;
  HANDLE file_handle_;
};
}  // namespace

void PutSmb::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  gsl_Expects(context && session);

  auto flow_file = session->get();
  if (!flow_file)
    context->yield();

  auto full_file_path = getFilePath(*context, flow_file);

  using UniqueFileHandle = std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&CloseHandle)>;
  DWORD creation_disposition = conflict_resolution_strategy_ == FileExistsResolutionStrategy::REPLACE_FILE ? CREATE_ALWAYS : CREATE_NEW;

  auto file_handle = UniqueFileHandle(CreateFileA(full_file_path.c_str(), GENERIC_WRITE, 0, nullptr, creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr), &CloseHandle);
  auto last_error = GetLastError();
  if (file_handle.get() == INVALID_HANDLE_VALUE && last_error == ERROR_FILE_EXISTS) {
    if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::FAIL_FLOW) {
      session->transfer(flow_file, Failure);
      return;
    } else if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::IGNORE_REQUEST) {
      session->transfer(flow_file, Success);
      return;
    }
  }

  if (file_handle.get() == INVALID_HANDLE_VALUE) {
    logger_->log_error("Could not open %s file for write due to %s", full_file_path, minifi::utils::OsUtils::windowsErrorToErrorCode(last_error).message());
    session->transfer(flow_file, Failure);
    return;
  }

  FlowFileToNetworkShareWriter flow_file_to_network_share_writer(file_handle.get());
  auto write_result = session->read(flow_file, flow_file_to_network_share_writer);
  if (io::isError(write_result)) {
    logger_->log_error("Failed to write flowfile onto network share");
    session->transfer(flow_file, Failure);
    return;
  }

  session->transfer(flow_file, Success);
}

}  // namespace org::apache::nifi::minifi::extensions::smb
