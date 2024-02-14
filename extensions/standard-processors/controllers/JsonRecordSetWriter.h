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

#include "PropertyDefinitionBuilder.h"
#include "Record.h"
#include "controllers/RecordSetWriter.h"
#include "core/FlowFile.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::standard {

class JsonRecordSetWriter final : public core::RecordSetWriter {
 public:
  explicit JsonRecordSetWriter(const std::string& name, const utils::Identifier& uuid = {});
  explicit JsonRecordSetWriter(const std::string& name, const std::shared_ptr<Configure>& configuration);

  ~JsonRecordSetWriter() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Writes the results of a RecordSet as either a JSON Array or one JSON object per line. "
    "If using Array output, then even if the RecordSet consists of a single row, it will be written as an array with a single element. "
    "If using One Line Per Object output, the JSON objects cannot be pretty-printed.";

  EXTENSIONAPI static constexpr auto SchemaWriteStrategy = core::PropertyDefinitionBuilder<>::createProperty("Schema Write Strategy")
      .withDescription("Specifies how the schema for a Record should be added to the data.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto OutputGrouping = core::PropertyDefinitionBuilder<>::createProperty("Output Grouping")
    .withDescription("Specifies how the writer should output the JSON records (as an array or one object per line, e.g.) Note that if 'One Line Per Object' is selected, then Pretty Print JSON must be false.")
    .supportsExpressionLanguage(true)
    .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 1>{
    SchemaWriteStrategy,
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  using RecordSetWriter::RecordSetWriter;

  void write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) override;

 private:
  void convertRecord(const core::Record& record, rapidjson::Value& record_json, rapidjson::Document::AllocatorType& alloc);

 public:
  void yield() override {}
  bool isRunning() const override {    return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }
};

}  // namespace org::apache::nifi::minifi::standard