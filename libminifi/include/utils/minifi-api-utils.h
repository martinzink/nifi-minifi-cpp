// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <string_view>

#include "core/extension/Extension.h"
#include "minifi-api.h"
#include "utils/minifi-api-legacy.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessSession.h"
#include "minifi-cpp/core/controller/ControllerServiceContext.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/io/OutputStream.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

inline minifi_string_view minifiStringView(const std::string_view s) {
  return minifi_string_view{.data = s.data(), .length = s.size()};
}

inline std::string toString(minifi_string_view sv) {
  return {sv.data, sv.length};
}

inline std::string_view toStringView(minifi_string_view sv) {
  return {sv.data, sv.length};
}

// Bidirectional type map between the C API's opaque handle types and their
// underlying C++ implementation types. A missing specialization at a call site
// is intentionally a compile error - any new opaque handle must be registered
// here before it can be passed through the generic cast helpers below.
template<typename C>
struct CppFor;
template<typename Cpp>
struct CFor;

#define MINIFI_API_MAP(CType, CppType) \
  template<>                           \
  struct CppFor<CType> {               \
    using type = CppType;              \
  };                                   \
  template<>                           \
  struct CFor<CppType> {               \
    using type = CType;                \
  }

MINIFI_API_MAP(minifi_controller_service_context, minifi::core::controller::ControllerServiceContext);
MINIFI_API_MAP(minifi_input_stream, minifi::io::InputStream);
MINIFI_API_MAP(minifi_output_stream, minifi::io::OutputStream);
MINIFI_API_MAP(minifi_extension, minifi::core::extension::Extension);
MINIFI_API_MAP(minifi_extension_context, minifi::core::extension::Extension::Context);
MINIFI_API_MAP(minifi_logger, std::shared_ptr<minifi::core::logging::Logger>);
MINIFI_API_MAP(minifi_flow_file, std::shared_ptr<minifi::core::FlowFile>);

#undef MINIFI_API_MAP

// Generic direct-pointer casts. Only usable for types registered above -
// otherwise the CppFor/CFor lookup fails to compile.
template<typename C>
auto* toCpp(C* c_ptr) noexcept {
  return reinterpret_cast<typename CppFor<C>::type*>(c_ptr);
}

template<typename Cpp>
auto* toC(Cpp* cpp_ptr) noexcept {
  return reinterpret_cast<typename CFor<Cpp>::type*>(cpp_ptr);
}

// Every processor context handle (schedule, trigger, and the v2 context and session) points to this
struct CProcessorContext {
  minifi::core::ProcessContext& context;
  minifi::core::ProcessSession* session;
};

inline minifi_schedule_context* toCScheduleContext(CProcessorContext* c_context) noexcept {
  return reinterpret_cast<minifi_schedule_context*>(c_context);
}

inline minifi_trigger_context* toCTriggerContext(CProcessorContext* c_context) noexcept {
  return reinterpret_cast<minifi_trigger_context*>(c_context);
}

inline minifi_process_context* toCLegacyContext(CProcessorContext* c_context) noexcept {
  return reinterpret_cast<minifi_process_context*>(c_context);
}

inline minifi_process_session* toCLegacySession(CProcessorContext* c_context) noexcept {
  return reinterpret_cast<minifi_process_session*>(c_context);
}

template<typename Handle>
requires std::same_as<Handle, minifi_schedule_context> || std::same_as<Handle, minifi_trigger_context>
    || std::same_as<Handle, minifi_process_context> || std::same_as<Handle, minifi_process_session>
CProcessorContext& toCProcessorContext(Handle* handle) {
  gsl_Assert(handle);
  return *reinterpret_cast<CProcessorContext*>(handle);
}

template<typename Handle>
minifi::core::ProcessContext& processContextOf(Handle* handle) {
  return toCProcessorContext(handle).context;
}

template<typename Handle>
minifi::core::ProcessSession& processSessionOf(Handle* handle) {
  auto* session = toCProcessorContext(handle).session;
  gsl_Assert(session);
  return *session;
}

// Converting through a generic function pointer keeps -Wcast-function-type-mismatch quiet for the deliberate
// reinterpretation of v2 processor callbacks
template<typename To, typename From>
To castFunctionPointer(From function) noexcept {
  using GenericFunction = void (*)();
  return reinterpret_cast<To>(reinterpret_cast<GenericFunction>(function));
}

inline minifi::core::FlowFile* toRawFlowFile(minifi_flow_file* flow_file) {
  if (auto cpp_flow_file = toCpp(flow_file)) {
    return cpp_flow_file->get();
  }
  return nullptr;
}

}  // namespace org::apache::nifi::minifi::utils
