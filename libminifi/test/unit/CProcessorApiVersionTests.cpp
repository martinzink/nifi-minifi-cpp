// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/Processor.h"
#include "minifi-api.h"
#include "minifi-cpp/core/ProcessorMetadata.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "utils/CProcessor.h"
#include "utils/minifi-api-legacy.h"
#include "utils/minifi-api-utils.h"

namespace minifi = org::apache::nifi::minifi;
using minifi::utils::minifiStringView;
using minifi::utils::toString;

namespace {

constexpr std::string_view GreetingPropertyName = "Greeting";
constexpr std::string_view GreetingDefault = "hello";
constexpr std::string_view GreetingAttribute = "greeting";
constexpr std::string_view SuccessName = "success";

// Signatures of the callbacks before API v3
using V2Trigger = minifi_status (*)(void*, minifi_process_context*, minifi_process_session*);
using V2Schedule = minifi_status (*)(void*, minifi_process_context*);

void storeString(void* user_ctx, minifi_string_view value) {
  *static_cast<std::string*>(user_ctx) = toString(value);
}

minifi_status v2Trigger(void*, minifi_process_context* context, minifi_process_session* session) {
  if (static_cast<void*>(context) != static_cast<void*>(session)) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
  std::string greeting;
  if (minifi_process_context_get_property(context, minifiStringView(GreetingPropertyName), nullptr, storeString, &greeting) != MINIFI_STATUS_SUCCESS) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
  auto* flow_file = minifi_process_session_get(session);
  if (!flow_file) {
    return MINIFI_STATUS_PROCESSOR_YIELD;
  }
  const auto greeting_view = minifiStringView(greeting);
  minifi_process_session_set_flow_file_attribute(session, flow_file, minifiStringView(GreetingAttribute), &greeting_view);
  return minifi_process_session_transfer(session, flow_file, minifiStringView(SuccessName));
}

minifi_status v3Trigger(void*, minifi_trigger_context* context) {
  auto* flow_file = minifi_trigger_context_get(context);
  if (!flow_file) {
    return MINIFI_STATUS_PROCESSOR_YIELD;
  }
  std::string greeting;
  if (minifi_trigger_context_get_property(context, minifiStringView(GreetingPropertyName), flow_file, storeString, &greeting) != MINIFI_STATUS_SUCCESS) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
  const auto greeting_view = minifiStringView(greeting);
  minifi_trigger_context_set_flow_file_attribute(context, flow_file, minifiStringView(GreetingAttribute), &greeting_view);
  return minifi_trigger_context_transfer(context, flow_file, minifiStringView(SuccessName));
}

std::unique_ptr<minifi::core::Processor> createProcessor(minifi_processor_callbacks callbacks, uint32_t api_version) {
  static const minifi_string_view greeting_default = minifiStringView(GreetingDefault);
  static const minifi_property_definition greeting_property{
    .name = minifiStringView(GreetingPropertyName),
    .display_name = minifiStringView(GreetingPropertyName),
    .description = minifiStringView("Greeting copied to the flow file"),
    .is_required = true,
    .is_sensitive = false,
    .default_value = &greeting_default,
    .allowed_values_count = 0,
    .allowed_values_ptr = nullptr,
    .validator = MINIFI_VALIDATOR_ALWAYS_VALID,
    .allowed_type = nullptr,
    .supports_expression_language = false
  };
  static const minifi_relationship_definition success{.name = minifiStringView(SuccessName), .description = minifiStringView("success")};

  const minifi_processor_definition definition{
    .full_name = minifiStringView("org::apache::nifi::minifi::test::GreetingProcessor"),
    .description = minifiStringView("Copies the greeting property to an attribute"),
    .properties_count = 1,
    .properties_ptr = &greeting_property,
    .dynamic_properties_count = 0,
    .dynamic_properties_ptr = nullptr,
    .relationships_count = 1,
    .relationships_ptr = &success,
    .output_attributes_count = 0,
    .output_attributes_ptr = nullptr,
    .supports_dynamic_properties = false,
    .supports_dynamic_relationships = false,
    .input_requirement = MINIFI_INPUT_REQUIRED,
    .is_single_threaded = true,
    .callbacks = callbacks
  };

  const minifi::core::ProcessorMetadata metadata{minifi::utils::Identifier{}, "GreetingProcessor",
      minifi::core::logging::LoggerFactory<minifi::utils::CProcessor>::getLogger()};
  std::unique_ptr<minifi::core::ProcessorApi> processor_impl;
  minifi::utils::useCProcessorClassDescription(definition, [&](const auto&, auto c_description) {
    c_description.api_version = api_version;
    processor_impl = std::make_unique<minifi::utils::CProcessor>(std::move(c_description), metadata);
  });
  return std::make_unique<minifi::core::Processor>("GreetingProcessor", metadata.name, metadata.uuid, std::move(processor_impl));
}

minifi_status v2Schedule(void*, minifi_process_context* context) {
  std::string greeting;
  if (minifi_process_context_get_property(context, minifiStringView(GreetingPropertyName), nullptr, storeString, &greeting) != MINIFI_STATUS_SUCCESS
      || greeting != GreetingDefault) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
  return minifi_process_context_set_trigger_when_empty(context, false);
}

minifi_status v3Schedule(void*, minifi_schedule_context* context) {
  std::string greeting;
  if (minifi_schedule_context_get_property(context, minifiStringView(GreetingPropertyName), storeString, &greeting) != MINIFI_STATUS_SUCCESS
      || greeting != GreetingDefault) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
  return minifi_schedule_context_set_trigger_when_empty(context, false);
}

minifi_processor_callbacks makeCallbacks(decltype(minifi_processor_callbacks::trigger) trigger, decltype(minifi_processor_callbacks::schedule) schedule) {
  return minifi_processor_callbacks{
    .create = [](minifi_processor_metadata) -> void* { return nullptr; },
    .destroy = [](void*) {},
    .trigger = trigger,
    .schedule = schedule,
    .unschedule = [](void*) {}
  };
}

minifi_processor_callbacks v2Callbacks() {
  return makeCallbacks(reinterpret_cast<decltype(minifi_processor_callbacks::trigger)>(static_cast<V2Trigger>(v2Trigger)),
      reinterpret_cast<decltype(minifi_processor_callbacks::schedule)>(static_cast<V2Schedule>(v2Schedule)));
}

}  // namespace

TEST_CASE("CProcessor schedules and triggers v2 extensions through the legacy functions") {
  minifi::test::SingleProcessorTestController controller{createProcessor(v2Callbacks(), 2)};
  const auto result = controller.trigger("input");
  const auto& transferred = result.at(minifi::core::Relationship{std::string{SuccessName}, ""});
  REQUIRE(transferred.size() == 1);
  CHECK(transferred[0]->getAttribute(GreetingAttribute) == GreetingDefault);
}

TEST_CASE("CProcessor schedules and triggers current extensions with the schedule and trigger contexts") {
  minifi::test::SingleProcessorTestController controller{createProcessor(makeCallbacks(v3Trigger, v3Schedule), MINIFI_API_VERSION)};
  const auto result = controller.trigger("input");
  const auto& transferred = result.at(minifi::core::Relationship{std::string{SuccessName}, ""});
  REQUIRE(transferred.size() == 1);
  CHECK(transferred[0]->getAttribute(GreetingAttribute) == GreetingDefault);
}
