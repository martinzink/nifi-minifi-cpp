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
#include "utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OsUtils.h"
#include "utils/file/FileWriterCallback.h"

namespace org::apache::nifi::minifi::extensions::smb {

void PutSmb::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
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
  conflict_resolution_strategy_ = utils::parseEnumProperty<put_smb::FileExistsResolutionStrategy>(*context, ConflictResolution);
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
    if (conflict_resolution_strategy_ == put_smb::FileExistsResolutionStrategy::FAIL_FLOW) {
      session->transfer(flow_file, Failure);
      return;
    } else if (conflict_resolution_strategy_ == put_smb::FileExistsResolutionStrategy::IGNORE_REQUEST) {
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
