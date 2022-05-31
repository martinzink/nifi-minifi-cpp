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


#include "ElasticSearchCredentialsControllerService.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {
const core::Property ElasticSearchCredentialsControllerService::Hosts(core::PropertyBuilder::createProperty("Hosts")
    ->withDescription("A comma-separated list of HTTP hosts that host Elasticsearch query nodes.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticSearchCredentialsControllerService::Username(core::PropertyBuilder::createProperty("Username")
    ->withDescription("The username to use with XPack security.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticSearchCredentialsControllerService::Password(core::PropertyBuilder::createProperty("Password")
    ->withDescription("The password to use with XPack security.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticSearchCredentialsControllerService::ConnectionTimeout(core::PropertyBuilder::createProperty("Connection timeout")
    ->withDescription("Controls the amount of time, in milliseconds, before a timeout occurs when trying to connect.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticSearchCredentialsControllerService::ReadTimeout(core::PropertyBuilder::createProperty("Read timeout")
    ->withDescription("Controls the amount of time, in milliseconds, before a timeout occurs when waiting for a response.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticSearchCredentialsControllerService::RetryTimeout(core::PropertyBuilder::createProperty("Retry timeout")
    ->withDescription("Controls the amount of time, in milliseconds, before a timeout occurs when retrying the operation.")
    ->supportsExpressionLanguage(true)
    ->build());

void ElasticSearchCredentialsControllerService::initialize() {
  setSupportedProperties({Hosts, Username, Password, ConnectionTimeout, ReadTimeout, RetryTimeout});
}

void ElasticSearchCredentialsControllerService::onEnable() {
  CredentialsLocation credentials_location;
  if (!getProperty(CredentialsLoc.getName(), credentials_location)) {
    logger_->log_error("Invalid Credentials Location, defaulting to %s", toString(CredentialsLocation::USE_DEFAULT_CREDENTIALS));
    credentials_location = CredentialsLocation::USE_DEFAULT_CREDENTIALS;
  }
  if (credentials_location == CredentialsLocation::USE_DEFAULT_CREDENTIALS) {
    credentials_ = createDefaultCredentials();
  } else if (credentials_location == CredentialsLocation::USE_COMPUTE_ENGINE_CREDENTIALS) {
    credentials_ = gcs::oauth2::CreateComputeEngineCredentials();
  } else if (credentials_location == CredentialsLocation::USE_JSON_FILE) {
    credentials_ = createCredentialsFromJsonPath();
  } else if (credentials_location == CredentialsLocation::USE_JSON_CONTENTS) {
    credentials_ = createCredentialsFromJsonContents();
  } else if (credentials_location == CredentialsLocation::USE_ANONYMOUS_CREDENTIALS) {
    credentials_ = gcs::oauth2::CreateAnonymousCredentials();
  }
  if (!credentials_)
    logger_->log_error("Couldn't create valid credentials");
}


}  // org::apache::nifi::minifi::extensions::elasticsearch
