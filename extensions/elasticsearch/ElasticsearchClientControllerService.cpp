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


#include "ElasticsearchClientControllerService.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {
const core::Property ElasticsearchClientControllerService::Hosts(core::PropertyBuilder::createProperty("Hosts")
                                                                          ->withDescription("A comma-separated list of HTTP hosts that host Elasticsearch query nodes. Currently only supports a single host.")
                                                                          ->supportsExpressionLanguage(true)
                                                                          ->build());

const core::Property ElasticsearchClientControllerService::Username(core::PropertyBuilder::createProperty("Username")
                                                                             ->withDescription("The username to use with XPack security.")
                                                                             ->supportsExpressionLanguage(true)
                                                                             ->build());

const core::Property ElasticsearchClientControllerService::Password(core::PropertyBuilder::createProperty("Password")
                                                                             ->withDescription("The password to use with XPack security.")
                                                                             ->supportsExpressionLanguage(true)
                                                                             ->build());

const core::Property ElasticsearchClientControllerService::SSLContext(core::PropertyBuilder::createProperty("SSL Context Service")
                                                                               ->withDescription("The SSL Context Service used to provide client certificate "
                                                                                                 "information for TLS/SSL (https) connections.")
                                                                               ->isRequired(false)
                                                                               ->asType<minifi::controllers::SSLContextService>()->build());

const core::Property ElasticsearchClientControllerService::ConnectionTimeout(core::PropertyBuilder::createProperty("Connection timeout")
                                                                                      ->withDescription("The time before a timeout occurs when trying to connect.")
                                                                                      ->withType(core::StandardValidators::get().TIME_PERIOD_VALIDATOR)
                                                                                      ->build());

const core::Property ElasticsearchClientControllerService::ReadTimeout(core::PropertyBuilder::createProperty("Read timeout")
                                                                                ->withDescription("The time before a timeout occurs when waiting for a response.")
                                                                                ->withType(core::StandardValidators::get().TIME_PERIOD_VALIDATOR)
                                                                                ->build());

void ElasticsearchClientControllerService::initialize() {
  setSupportedProperties({Hosts, Username, Password, ConnectionTimeout, ReadTimeout, SSLContext});
}

void ElasticsearchClientControllerService::onEnable(core::controller::ControllerServiceProvider* controller_service_provider) {
  gsl_Expects(controller_service_provider);
  std::string hosts_str;
  getProperty(Hosts.getName(), hosts_str);
  auto hosts = utils::StringUtils::split(hosts_str, ",");
  if (hosts.size() < 1)
    throw std::invalid_argument("Invalid hosts");
  client_.initialize("POST", hosts[0], getSSLContextService(*controller_service_provider));
  core::TimePeriodValue time_period;
  if (getProperty(ConnectionTimeout.getName(), time_period))
    client_.setConnectionTimeout(time_period.getMilliseconds());
  if (getProperty(ReadTimeout.getName(), time_period))
    client_.setReadTimeout(time_period.getMilliseconds());
}

std::shared_ptr<minifi::controllers::SSLContextService> ElasticsearchClientControllerService::getSSLContextService(core::controller::ControllerServiceProvider& controller_service_provider) const {
  std::string context_name;
  if (getProperty(SSLContext.getName(), context_name) && !IsNullOrEmpty(context_name))
    return std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(controller_service_provider.getControllerService(context_name));
  return nullptr;
}


}  // org::apache::nifi::minifi::extensions::elasticsearch
