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

#include "controllers/RecordSetReader.h"

namespace org::apache::nifi::minifi::core {

core::RecordSchemaAccessStrategy RecordSetReader::getAccessStrategy() const {
  auto access_strat = getProperty(SchemaAccessStrategy);
  if (!access_strat)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("{} is missing", SchemaAccessStrategy.name));
  auto result = magic_enum::enum_cast<core::RecordSchemaAccessStrategy>(*access_strat);
  if (!result) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Property {} has invalid value: {}", SchemaAccessStrategy.name, *access_strat));
  }
  return result.value();
}

}  // namespace org::apache::nifi::minifi::core

