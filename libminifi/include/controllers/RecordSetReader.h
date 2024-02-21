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
#pragma once

#include "core/controller/ControllerService.h"

#include "core/FlowFile.h"
#include "core/ProcessSession.h"
#include "Record.h"

namespace org::apache::nifi::minifi::core {

enum class RecordSchemaAccessStrategy {
 InferSchema,
 UseSchemaText
};

class RecordSetReader : public controller::ControllerService {
 public:
  using ControllerService::ControllerService;


  EXTENSIONAPI static constexpr auto SchemaAccessStrategy = PropertyDefinitionBuilder<>::createProperty("Schema Access Strategy")
     .withDescription("Specifies how to obtain the schema that is to be used for interpreting the data.")
     .supportsExpressionLanguage(true)
     .build();

  EXTENSIONAPI static constexpr auto SchemaText = PropertyDefinitionBuilder<>::createProperty("Schema Text")
    .withDescription("Json formatted text of the schema to be used")
    .supportsExpressionLanguage(true)
    .withDefaultValue("${schema.text}")
    .build();

  virtual nonstd::expected<RecordSet, std::error_code> read(const std::shared_ptr<FlowFile>& flow_file, ProcessSession& session, const RecordSchema* record_schema) = 0;
  virtual RecordSchema* getSchema(const ProcessContext& context) const = 0;
};

}  // namespace org::apache::nifi::minifi::core
