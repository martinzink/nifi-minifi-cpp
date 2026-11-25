// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "minifi-api.h"

// API v2 functions, still exported so extensions built against v2 keep working, but no longer part of the public header

#ifdef __cplusplus
extern "C" {
#endif

struct minifi_process_context;
struct minifi_process_session;

enum minifi_status minifi_process_context_set_trigger_when_empty(struct minifi_process_context*, bool trigger_when_empty);
enum minifi_status minifi_process_context_report_metrics(struct minifi_process_context*, size_t count, const struct minifi_string_view* metric_names,
    const double* metric_values);
enum minifi_status minifi_process_context_get_property(struct minifi_process_context* context, struct minifi_string_view property_name,
    MINIFI_NULLABLE struct minifi_flow_file* flowfile, void (*cb)(void* user_ctx, struct minifi_string_view property_value), void* user_ctx);
enum minifi_status minifi_process_context_get_controller_service_from_property(struct minifi_process_context* process_context,
    struct minifi_string_view property_name, struct minifi_string_view controller_service_type,
    struct minifi_controller_service** controller_service_out);
void minifi_process_context_get_dynamic_properties(struct minifi_process_context* context, MINIFI_NULLABLE struct minifi_flow_file* minifi_flow_file,
    void (*cb)(void* user_ctx, struct minifi_string_view dynamic_property_name, struct minifi_string_view dynamic_property_value), void* user_ctx);
MINIFI_OWNED struct minifi_flow_file* minifi_process_session_get(struct minifi_process_session*);
MINIFI_OWNED struct minifi_flow_file* minifi_process_session_create(struct minifi_process_session* session,
    MINIFI_NULLABLE struct minifi_flow_file* parent_flowfile);
enum minifi_status minifi_process_session_penalize(struct minifi_process_session* session, struct minifi_flow_file* flowfile);
enum minifi_status minifi_process_session_transfer(struct minifi_process_session* session, MINIFI_OWNED struct minifi_flow_file* flowfile,
    struct minifi_string_view relationship_name);
enum minifi_status minifi_process_session_remove(struct minifi_process_session* session, MINIFI_OWNED struct minifi_flow_file* flowfile);
enum minifi_status minifi_process_session_read(struct minifi_process_session*, struct minifi_flow_file*,
    int64_t (*cb)(void* user_ctx, struct minifi_input_stream*), void* user_ctx);
enum minifi_status minifi_process_session_write(struct minifi_process_session*, struct minifi_flow_file*,
    int64_t (*cb)(void* user_ctx, struct minifi_output_stream*), void* user_ctx);
enum minifi_status minifi_process_session_set_flow_file_attribute(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    struct minifi_string_view attribute_name, const struct minifi_string_view* attribute_value);
bool minifi_process_session_get_flow_file_attribute(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    struct minifi_string_view attribute_name, void (*cb)(void* user_ctx, struct minifi_string_view attribute_value), void* user_ctx);
void minifi_process_session_get_flow_file_attributes(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    void (*cb)(void* user_ctx, struct minifi_string_view attribute_name, struct minifi_string_view attribute_value), void* user_ctx);
uint64_t minifi_process_session_get_flow_file_size(struct minifi_process_session* session, struct minifi_flow_file* flowfile);
enum minifi_status minifi_process_session_get_flow_file_id(struct minifi_process_session* session, struct minifi_flow_file* flowfile,
    void (*cb)(void* user_ctx, struct minifi_string_view flow_file_id), void* user_ctx);
enum minifi_status minifi_process_context_get_ssl_data_from_property(struct minifi_process_context* process_context,
    struct minifi_string_view property_name, void (*cb)(void* user_ctx, const struct minifi_ssl_data* ssl_data), void* user_ctx);
enum minifi_status minifi_process_context_get_proxy_data_from_property(struct minifi_process_context* process_context,
    struct minifi_string_view property_name, void (*cb)(void* user_ctx, const struct minifi_proxy_data* proxy_data), void* user_ctx);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
