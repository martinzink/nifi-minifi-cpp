// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <stdexcept>

#include "minifi-api.h"

extern "C" {
minifi_extension* MINIFI_REGISTER_EXTENSION_FN(minifi_extension_context*, const minifi_extension_definition*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_register_processor(minifi_extension*, const minifi_processor_definition*) {
  throw std::runtime_error("Not implemented");
}

minifi_status minifi_register_controller_service(minifi_extension*, const minifi_controller_service_definition*) {
  throw std::runtime_error("Not implemented");
}

void minifi_logger_log_string(minifi_logger*, minifi_log_level, minifi_string_view) {
  throw std::runtime_error("Not implemented");
}

bool minifi_logger_should_log(minifi_logger*, minifi_log_level) {
  throw std::runtime_error("Not implemented");
}

void minifi_config_get(minifi_extension_context*, minifi_string_view, void (*)(void* user_ctx, minifi_string_view config_value), void*) {
  throw std::runtime_error("Not implemented");
}

size_t minifi_input_stream_size(minifi_input_stream*) {
  throw std::runtime_error("Not implemented");
}

int64_t minifi_input_stream_read(minifi_input_stream*, char*, size_t) {
  throw std::runtime_error("Not implemented");
}

int64_t minifi_output_stream_write(minifi_output_stream*, const char*, size_t) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_controller_service_context_get_property(struct minifi_controller_service_context*, minifi_string_view,
    void (*)(void* user_ctx, minifi_string_view property_value), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_schedule_context_set_trigger_when_empty(struct minifi_schedule_context*, bool) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_schedule_context_get_property(struct minifi_schedule_context*, struct minifi_string_view,
    void (*)(void* user_ctx, struct minifi_string_view property_value), void*) {
  throw std::runtime_error("Not implemented");
}

void minifi_schedule_context_get_dynamic_properties(struct minifi_schedule_context*,
    void (*)(void* user_ctx, struct minifi_string_view dynamic_property_name, struct minifi_string_view dynamic_property_value), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_schedule_context_get_controller_service_from_property(struct minifi_schedule_context*,
    struct minifi_string_view, struct minifi_string_view, struct minifi_controller_service**) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_schedule_context_get_ssl_data_from_property(struct minifi_schedule_context*, struct minifi_string_view,
    void (*)(void* user_ctx, const struct minifi_ssl_data* ssl_data), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_schedule_context_get_proxy_data_from_property(struct minifi_schedule_context*,
    struct minifi_string_view, void (*)(void* user_ctx, const struct minifi_proxy_data* proxy_data), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_report_metrics(struct minifi_trigger_context*, size_t, const struct minifi_string_view*,
    const double*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_get_property(struct minifi_trigger_context*, struct minifi_string_view,
    MINIFI_NULLABLE struct minifi_flow_file*, void (*)(void* user_ctx, struct minifi_string_view property_value), void*) {
  throw std::runtime_error("Not implemented");
}

void minifi_trigger_context_get_dynamic_properties(struct minifi_trigger_context*, MINIFI_NULLABLE struct minifi_flow_file*,
    void (*)(void* user_ctx, struct minifi_string_view dynamic_property_name, struct minifi_string_view dynamic_property_value), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_get_controller_service_from_property(struct minifi_trigger_context*,
    struct minifi_string_view, struct minifi_string_view, struct minifi_controller_service**) {
  throw std::runtime_error("Not implemented");
}

MINIFI_OWNED struct minifi_flow_file* minifi_trigger_context_get(struct minifi_trigger_context*) {
  throw std::runtime_error("Not implemented");
}

MINIFI_OWNED struct minifi_flow_file* minifi_trigger_context_create(struct minifi_trigger_context*,
    MINIFI_NULLABLE struct minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_penalize(struct minifi_trigger_context*, struct minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_transfer(struct minifi_trigger_context*, MINIFI_OWNED struct minifi_flow_file*,
    struct minifi_string_view) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_remove(struct minifi_trigger_context*, MINIFI_OWNED struct minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_read(struct minifi_trigger_context*, struct minifi_flow_file*,
    int64_t (*)(void* user_ctx, struct minifi_input_stream*), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_write(struct minifi_trigger_context*, struct minifi_flow_file*,
    int64_t (*)(void* user_ctx, struct minifi_output_stream*), void*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_set_flow_file_attribute(struct minifi_trigger_context*, struct minifi_flow_file*,
    struct minifi_string_view, const struct minifi_string_view*) {
  throw std::runtime_error("Not implemented");
}

bool minifi_trigger_context_get_flow_file_attribute(struct minifi_trigger_context*, struct minifi_flow_file*,
    struct minifi_string_view, void (*)(void* user_ctx, struct minifi_string_view attribute_value), void*) {
  throw std::runtime_error("Not implemented");
}

void minifi_trigger_context_get_flow_file_attributes(struct minifi_trigger_context*, struct minifi_flow_file*,
    void (*)(void* user_ctx, struct minifi_string_view attribute_name, struct minifi_string_view attribute_value), void*) {
  throw std::runtime_error("Not implemented");
}

uint64_t minifi_trigger_context_get_flow_file_size(struct minifi_trigger_context*, struct minifi_flow_file*) {
  throw std::runtime_error("Not implemented");
}

enum minifi_status minifi_trigger_context_get_flow_file_id(struct minifi_trigger_context*, struct minifi_flow_file*,
    void (*)(void* user_ctx, struct minifi_string_view flow_file_id), void*) {
  throw std::runtime_error("Not implemented");
}

}  // extern "C"
