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


#include "ElasticsearchCredentialsControllerService.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {
const core::Property ElasticsearchCredentialsControllerService::Username(core::PropertyBuilder::createProperty("Username")
    ->withDescription("The username for basic authentication")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticsearchCredentialsControllerService::Password(core::PropertyBuilder::createProperty("Password")
    ->withDescription("The password for basic authentication")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticsearchCredentialsControllerService::CredentialsType(core::PropertyBuilder::createProperty("Credentials Type")
    ->withDescription("The location of the credentials.")
    ->withAllowableValues(CredType::values())
    ->withDefaultValue(toString(CredType::USE_API_KEY))
    ->isRequired(true)
    ->build());

const core::Property ElasticsearchCredentialsControllerService::ApiKey(core::PropertyBuilder::createProperty("Password")
    ->withDescription("The API Key to use")
    ->build());


void ElasticsearchCredentialsControllerService::initialize() {
  setSupportedProperties({Username, Password, CredentialsType, ApiKey});
}

void ElasticsearchCredentialsControllerService::onEnable(core::controller::ControllerServiceProvider*) {
  getProperty(CredentialsType.getName(), cred_type_);

  if (cred_type_ == CredType::USE_API_KEY) {
    std::string api_key;
    getProperty(ApiKey.getName(), api_key);
    api_key_.emplace(std::move(api_key));
  } else if (cred_type_ == CredType::USE_BASIC_AUTHENTICATION) {
    std::string username, password;
    getProperty(Username.getName(), username);
    getProperty(Password.getName(), password);
    username_password_.emplace(std::move(username), std::move(password));
  }
}

void ElasticsearchCredentialsControllerService::authenticateClient(utils::HTTPClient& client) {
  if (cred_type_ == CredType::USE_API_KEY) {
    gsl_Expects(api_key_);
    client.appendHeader("Authorization", "ApiKey " + *api_key_);
  } else if (cred_type_ == CredType::USE_BASIC_AUTHENTICATION) {
    gsl_Expects(username_password_);
    client.setBasicAuth(username_password_->first, username_password_->second);
  }
}

REGISTER_RESOURCE(ElasticsearchCredentialsControllerService, "Elasticsearch Credentials Controller Service");
}  // org::apache::nifi::minifi::extensions::elasticsearch