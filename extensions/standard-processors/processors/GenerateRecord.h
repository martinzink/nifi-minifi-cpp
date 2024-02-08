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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class GenerateRecord : public core::Processor {
 public:
  explicit GenerateRecord(const std::string_view name, const utils::Identifier& uuid = {}) // NOLINT
      : Processor(name, uuid) {
  }
  ~GenerateRecord() override = default;

  EXTENSIONAPI static constexpr const char* Description = "This processor creates FlowFiles with records having random value for the specified fields. "
      "GenerateRecord is useful for testing, configuration, and simulation. "
      "It uses either user-defined properties to define a record schema or a provided schema and generates the specified number of records using random data for the fields in the schema.";

  EXTENSIONAPI static constexpr auto NumberOfRecords = core::PropertyDefinitionBuilder<>::createProperty("Number of Records")
      .withDescription("Specifies how many records will be generated for each outgoing FlowFile.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("100")
      .build();

  EXTENSIONAPI static constexpr auto RecordWriter = core::PropertyDefinitionBuilder<>::createProperty("Record Writer")
      .withDescription("Specifies the Controller Service to use for writing out the records")
      .isRequired(true)
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 5>{
      FileSize,
      BatchSize,
      DataFormat,
      UniqueFlowFiles,
      CustomText
  };

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  EXTENSIONAPI static const char *DATA_FORMAT_TEXT;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& sessionFactory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  void refreshNonUniqueData(core::ProcessContext& context);

 private:
  enum class Mode {
    UniqueByte,
    UniqueText,
    NotUniqueByte,
    NotUniqueText,
    CustomText,
    Empty
  };

  Mode mode_;

  std::vector<char> non_unique_data_;

  uint64_t batch_size_{1};
  uint64_t file_size_{1024};

  static Mode getMode(bool is_unique, bool is_text, bool has_custom_text, uint64_t file_size);
  static bool isUnique(Mode mode) { return mode == Mode::UniqueText || mode == Mode::UniqueByte; }
  static bool isText(Mode mode) { return mode == Mode::UniqueText || mode == Mode::CustomText || mode == Mode::NotUniqueText; }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GenerateFlowFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
