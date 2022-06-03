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

namespace org::apache::nifi::minifi::extensions::elasticsearch {
const core::Property ElasticsearchCredentialsControllerService::Username(core::PropertyBuilder::createProperty("Username")
    ->withDescription("The username to use with XPack security.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property ElasticsearchCredentialsControllerService::Password(core::PropertyBuilder::createProperty("Password")
    ->withDescription("The password to use with XPack security.")
    ->supportsExpressionLanguage(true)
    ->build());

void ElasticsearchCredentialsControllerService::initialize() {
  setSupportedProperties({Username, Password});
}

void authenticateClient(utils::HTTPClient& client) {
  // TODO
}

}  // org::apache::nifi::minifi::extensions::elasticsearch
