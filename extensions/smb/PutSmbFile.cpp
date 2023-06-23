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

#include "PutSmbFile.hpp"
#include "core/PropertyBuilder.h"


namespace org::apache::nifi::minifi::extensions::smb {

const core::Property PutSmbFile::Hostname(
    core::PropertyBuilder::createProperty("Hostname")
      ->withDescription("The network host to which files should be written.")
      ->isRequired(true)
      ->build());

const core::Property PutSmbFile::Share(
    core::PropertyBuilder::createProperty("Share")
      ->withDescription(R"(The network share to which files should be written. This is the "first folder" after the hostname: \\hostname\[share]\dir1\dir2)")
      ->isRequired(true)
      ->build());

const core::Property PutSmbFile::Directory(
    core::PropertyBuilder::createProperty("Directory")
      ->withDescription(R"(The network folder to which files should be written. This is the remaining relative path after the share \\hostname\share\[dir1\dir2]. You may use expression language.)")
      ->isRequired(false)
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property PutSmbFile::Domain(
    core::PropertyBuilder::createProperty("Domain")
      ->withDescription("The domain used for authentication. Optional, in most cases username and password is sufficient.")
      ->isRequired(false)
      ->build());

const core::Property PutSmbFile::Username(
    core::PropertyBuilder::createProperty("Username")
      ->withDescription("The username used for authentication. If no username is set then anonymous authentication is attempted.")
      ->isRequired(false)
      ->build());

const core::Property PutSmbFile::Password(
    core::PropertyBuilder::createProperty("Password")
      ->withDescription("The password used for authentication. Required if Username is set.")
      ->isRequired(false)
      ->build());

const core::Property PutSmbFile::CreateMissingDirectories(
    core::PropertyBuilder::createProperty("Create Missing Directories")
      ->withDescription("If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.")
      ->withDefaultValue(true)
      ->isRequired(true)
      ->build());

const core::Property PutSmbFile::ShareAccessStrategy(
    core::PropertyBuilder::createProperty("Share Access Strategy")
      ->withDescription("Indicates which shared access are granted on the file during the write. None is the most restrictive, but the safest setting to prevent corruption.")
      ->withAllowableValues(ShareAccessStrategies::getValues())
      ->withDefaultValue(ShareAccessStrategies::kNone)
      ->isRequired(true)
      ->build());

const core::Property PutSmbFile::ConflictResolution(
    core::PropertyBuilder::createProperty("Conflict Resolution Strategy")
      ->withDescription("Indicates what should happen when a file with the same name already exists in the output directory")
      ->withAllowableValues(ConflictResolutionStrategies::getValues())
      ->withDefaultValue(ConflictResolutionStrategies::kReplace)
      ->isRequired(true)
      ->build());

const core::Property PutSmbFile::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")
    ->withDescription("The maximum number of files to put in each iteration")
    ->withDefaultValue<uint64_t>(100)
    ->isRequired(true)
    ->build());


const core::Property PutSmbFile::TemporarySuffix(
    core::PropertyBuilder::createProperty("Temporary Suffix")
      ->withDescription("A temporary suffix which will be apended to the filename while it's transfering. After the transfer is complete, the suffix will be removed.")
      ->isRequired(false)
      ->build());

const core::Property PutSmbFile::SmbDialect(
    core::PropertyBuilder::createProperty("SMB Dialect")
      ->withDescription("The SMB dialect is negotiated between the client and the server by default to the highest common version supported by both end. "
                        "In some rare cases, the client-server communication may fail with the automatically negotiated dialect. "
                        "This property can be used to set the dialect explicitly (e.g. to downgrade to a lower version), when those situations would occur.")
      ->withAllowableValues(SmbDialects::getValues())
      ->withDefaultValue(SmbDialects::kAuto)
      ->isRequired(true)
      ->build());

const core::Property PutSmbFile::UseEncryption(
    core::PropertyBuilder::createProperty("Use Encryption")
      ->withDescription("Turns on/off encrypted communication between the client and the server. "
                        "The property's behavior is SMB dialect dependent: SMB 2.x does not support encryption and the property has no effect. "
                        "In case of SMB 3.x, it is a hint/request to the server to turn encryption on if the server also supports it.")
      ->withDefaultValue(false)
      ->isRequired(true)
      ->build());

}  // namespace org::apache::nifi::minifi::extensions::smb