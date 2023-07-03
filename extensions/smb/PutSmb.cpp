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

#include "PutSmb.h"
#include "SmbConnectionControllerService.h"
#include "core/PropertyBuilder.h"
#include "utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OsUtils.h"
#include "utils/file/FileWriterCallback.h"

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

void PutSmb::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  gsl_Expects(context && session && smb_connection_controller_service_);

  auto connection_error = smb_connection_controller_service_->validateConnection();
  if (connection_error) {
    logger_->log_error("Couldn't establish connection to the specified network location due to %s", connection_error.message());
    context->yield();
    return;
  }

  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto full_file_path = getFilePath(*context, flow_file);

  if (utils::file::exists(full_file_path)) {
    logger_->log_warn("Destination file %s exists; applying Conflict Resolution Strategy: %s", full_file_path.string(), conflict_resolution_strategy_.toString());
    if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::FAIL_FLOW) {
      session->transfer(flow_file, Failure);
      return;
    } else if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::IGNORE_REQUEST) {
      session->transfer(flow_file, Success);
      return;
    }
  }

  if (!utils::file::exists(full_file_path.parent_path()) && create_missing_dirs_) {
    logger_->log_debug("Destination directory does not exist; will attempt to create: %s", full_file_path.parent_path().string());
    utils::file::create_dir(full_file_path.parent_path(), true);
  }

  bool success = false;

  utils::FileWriterCallback file_writer_callback(full_file_path);
  auto read_result = session->read(flow_file, std::ref(file_writer_callback));
  if (io::isError(read_result)) {
    logger_->log_error("Failed to write to %s", full_file_path.string());
    success = false;
  } else {
    success = file_writer_callback.commit();
  }

  session->transfer(flow_file, success ? Success : Failure);
}

}  // namespace org::apache::nifi::minifi::extensions::smb
