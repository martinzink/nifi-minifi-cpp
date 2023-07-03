/**
 * @file PutFile.cpp
 * PutFile class implementation
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

#include "PutFile.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include "utils/file/FileUtils.h"
#include "utils/file/FileWriterCallback.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

std::shared_ptr<utils::IdGenerator> PutFile::id_generator_ = utils::IdGenerator::getIdGenerator();

#ifndef WIN32
const core::Property PutFile::Permissions(
    core::PropertyBuilder::createProperty("Permissions")
      ->withDescription("Sets the permissions on the output file to the value of this attribute. "
                        "Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.")
      ->build());
const core::Property PutFile::DirectoryPermissions(
    core::PropertyBuilder::createProperty("Directory Permissions")
      ->withDescription("Sets the permissions on the directories being created if 'Create Missing Directories' property is set. "
                        "Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.")
      ->build());
#endif

const core::Property PutFile::Directory(
    core::PropertyBuilder::createProperty("Directory")->withDescription("The output directory to which to put files")->supportsExpressionLanguage(true)->withDefaultValue(".")->build());

const core::Property PutFile::ConflictResolution(
    core::PropertyBuilder::createProperty("Conflict Resolution Strategy")
        ->withDescription("Indicates what should happen when a file with the same name already exists in the output directory")
        ->withAllowableValues(FileExistsResolutionStrategy::values())
        ->withDefaultValue(toString(FileExistsResolutionStrategy::REPLACE_FILE))
        ->isRequired(true)
        ->build());

const core::Property PutFile::CreateDirs("Create Missing Directories", "If true, then missing destination directories will be created. "
                                   "If false, flowfiles are penalized and sent to failure.",
                                   "true", true, "", { "Directory" }, { });

const core::Property PutFile::MaxDestFiles(
    core::PropertyBuilder::createProperty("Maximum File Count")->withDescription("Specifies the maximum number of files that can exist in the output directory")->withDefaultValue<int>(-1)->build());

const core::Relationship PutFile::Success("success", "All files are routed to success");
const core::Relationship PutFile::Failure("failure", "Failed files (conflict, write failure, etc.) are transferred to failure");

void PutFile::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void PutFile::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  conflict_resolution_strategy_ = utils::parseEnumProperty<FileExistsResolutionStrategy>(*context, ConflictResolution);
  try_mkdirs_ = context->getProperty<bool>(CreateDirs).value_or(true);
  if (auto max_dest_files = context->getProperty<int64_t>(MaxDestFiles); max_dest_files && *max_dest_files > 0) {
    max_dest_files_ = gsl::narrow_cast<uint64_t>(*max_dest_files);
  }

#ifndef WIN32
  getPermissions(context);
  getDirectoryPermissions(context);
#endif
}

std::optional<std::filesystem::path> PutFile::getDestinationPath(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  std::filesystem::path directory;
  if (auto directory_str = context.getProperty(Directory, flow_file); directory_str && !directory_str->empty()) {
    directory = *directory_str;
  } else {
    logger_->log_error("Directory attribute evaluated to invalid value");
    return std::nullopt;
  }
  auto file_name_str = flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(flow_file->getUUIDStr());

  return directory / file_name_str;
}

void PutFile::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::shared_ptr<core::FlowFile> flow_file = session->get();

  // Do nothing if there are no incoming files
  if (!flow_file) {
    return;
  }

  auto dest_path = getDestinationPath(*context, flow_file);
  if (!dest_path) {
    session->transfer(flow_file, Failure);
    return;
  }

  logger_->log_debug("PutFile writing file %s into directory %s", dest_path->filename().string(), dest_path->parent_path().string());

  if (max_dest_files_ && utils::file::is_directory(dest_path->parent_path())) {
    if (utils::file::countNumberOfFiles(dest_path->parent_path()) >= *max_dest_files_) {
      logger_->log_warn("Routing to failure because the output directory %s has at least %u files, which exceeds the "
                        "configured max number of files", dest_path->parent_path().string(), *max_dest_files_);
      session->transfer(flow_file, Failure);
      return;
    }
  }

  if (utils::file::exists(*dest_path)) {
    logger_->log_warn("Destination file %s exists; applying Conflict Resolution Strategy: %s", dest_path->string(), conflict_resolution_strategy_.toString());
    if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::FAIL_FLOW) {
      session->transfer(flow_file, Failure);
      return;
    } else if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::IGNORE_REQUEST) {
      session->transfer(flow_file, Success);
      return;
    }
  }

  putFile(*session, flow_file, *dest_path);
}

void PutFile::putFile(core::ProcessSession& session,
                      const std::shared_ptr<core::FlowFile>& flow_file,
                      const std::filesystem::path& dest_file) {
  if (!utils::file::exists(dest_file.parent_path()) && try_mkdirs_) {
    logger_->log_debug("Destination directory does not exist; will attempt to create: %s", dest_file.parent_path().string());
    utils::file::create_dir(dest_file.parent_path(), true);
#ifndef WIN32
    if (directory_permissions_.valid()) {
      utils::file::set_permissions(destDir, directory_permissions_.getValue());
    }
#endif
  }

  bool success = false;

  utils::FileWriterCallback file_writer_callback(dest_file);
  auto read_result = session.read(flow_file, std::ref(file_writer_callback));
  if (io::isError(read_result)) {
    logger_->log_error("Failed to write to %s", dest_file.string());
    success = false;
  } else {
    success = file_writer_callback.commit();
  }

#ifndef WIN32
  if (permissions_.valid()) {
    utils::file::set_permissions(dest_file, permissions_.getValue());
  }
#endif

  session.transfer(flow_file, success ? Success : Failure);
}

#ifndef WIN32
void PutFile::getPermissions(core::ProcessContext *context) {
  std::string permissions_str;
  context->getProperty(Permissions.getName(), permissions_str);
  if (permissions_str.empty()) {
    return;
  }

  try {
    permissions_.setValue(std::stoi(permissions_str, nullptr, 8));
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Permissions property is invalid");
  }

  if (!permissions_.valid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Permissions property is invalid: out of bounds");
  }
}

void PutFile::getDirectoryPermissions(core::ProcessContext *context) {
  std::string dir_permissions_str;
  context->getProperty(DirectoryPermissions.getName(), dir_permissions_str);
  if (dir_permissions_str.empty()) {
    return;
  }

  try {
    directory_permissions_.setValue(std::stoi(dir_permissions_str, nullptr, 8));
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Directory Permissions property is invalid");
  }

  if (!directory_permissions_.valid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Directory Permissions property is invalid: out of bounds");
  }
}
#endif

REGISTER_RESOURCE(PutFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
