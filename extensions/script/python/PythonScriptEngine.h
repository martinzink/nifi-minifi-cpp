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

#include "PythonBindings.h"
#include "PyException.h"

#include <mutex>
#include <memory>
#include <utility>
#include <exception>
#include <string>

#include "core/ProcessSession.h"
#include "core/Processor.h"

#include "../ScriptEngine.h"
#include "../ScriptProcessContext.h"
#include "PythonProcessor.h"
#include "types/PyProcessSession.h"
#include "../ScriptException.h"

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility push(hidden)
#endif

namespace org::apache::nifi::minifi::python {

#if defined(__GNUC__) || defined(__GNUG__)
class __attribute__((visibility("default"))) GlobalInterpreterLock {
#else
class GlobalInterpreterLock {
#endif
 public:
  GlobalInterpreterLock();
  ~GlobalInterpreterLock();

 private:
  PyGILState_STATE gil_state_;
};

struct Interpreter {
  Interpreter();
  ~Interpreter();

  Interpreter(const Interpreter& other) = delete;
  Interpreter(Interpreter&& other) = delete;
  Interpreter& operator=(const Interpreter& other) = delete;
  Interpreter& operator=(Interpreter&& other) = delete;

  PyThreadState* saved_thread_state_ = nullptr;
};

Interpreter* getInterpreter();

#if defined(__GNUC__) || defined(__GNUG__)
class __attribute__((visibility("default"))) PythonScriptEngine : public script::ScriptEngine {
#else
class PythonScriptEngine : public script::ScriptEngine {
#endif
 public:
  PythonScriptEngine();
  ~PythonScriptEngine() override;

  PythonScriptEngine(const PythonScriptEngine& other) = delete;
  PythonScriptEngine(PythonScriptEngine&& other) = delete;
  PythonScriptEngine& operator=(const PythonScriptEngine& other) = delete;
  PythonScriptEngine& operator=(PythonScriptEngine&& other) = delete;

  static void initialize() {}

  void eval(const std::string& script) override;
  void evalFile(const std::filesystem::path& file_name) override;

  template<typename... Args>
  void call(std::string_view fn_name, Args&& ...args) {
    GlobalInterpreterLock gil_lock;
    try {
      if (auto item = bindings_[fn_name]) {
        auto result = BorrowedCallable(*item)(std::forward<Args>(args)...);
        if (!result) {
          throw PyException();
        }
      }
    } catch (const std::exception& e) {
      throw minifi::script::ScriptException(e.what());
    }
  }

  template<typename ... Args>
  void callRequiredFunction(const std::string& fn_name, Args&& ...args) {
    GlobalInterpreterLock gil_lock;
    if (auto item = bindings_[fn_name]) {
      auto result = BorrowedCallable(*item)(std::forward<Args>(args)...);
      if (!result) {
        throw PyException();
      }
    } else {
      throw std::runtime_error("Required Function" + fn_name + " is not found within Python bindings");
    }
  }

  template<object::convertible T>
  void bind(const std::string& name, const T& value) {
    GlobalInterpreterLock gil_lock;
    bindings_.put(name, value);
  }

  void onInitialize(core::Processor* proc);
  void describe(core::Processor* proc);
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context);
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
 private:
  void evalInternal(std::string_view script);

  void evaluateModuleImports();
  OwnedDict bindings_;
};

}  // namespace org::apache::nifi::minifi::python

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC visibility pop
#endif
