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

#include <string>
#include <filesystem>

#include "LuaScriptExecutor.h"
#include "range/v3/range/conversion.hpp"
#include "Resource.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaScriptExecutor::LuaScriptExecutor(std::string name, const utils::Identifier& uuid) : script::ScriptExecutor(std::move(name), uuid) {}

void LuaScriptExecutor::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  auto lua_script_engine = lua_script_engine_queue_->getResource();

  if (module_directory_) {
    lua_script_engine->setModulePaths(utils::StringUtils::splitAndTrimRemovingEmpty(*module_directory_, ",") | ranges::to<std::vector<std::filesystem::path>>());
  }

  if (!script_body_.empty()) {
    lua_script_engine->eval(script_body_);
  } else if (!script_file_.empty()) {
    lua_script_engine->evalFile(script_file_);
  } else {
    throw std::runtime_error("Neither Script Body nor Script File is available to execute");
  }

  lua_script_engine->onTrigger(context, session);
}

void LuaScriptExecutor::initialize(std::filesystem::path script_file,
    std::string script_body,
    std::optional<std::string> module_directory,
    size_t max_concurrent_engines,
    const core::Relationship& success,
    const core::Relationship& failure,
    std::shared_ptr<core::logging::Logger> logger) {
  script_file_ = std::move(script_file);
  script_body_ = std::move(script_body);
  module_directory_ = std::move(module_directory);

  auto create_engine = [=]() -> std::unique_ptr<LuaScriptEngine> {
    auto engine = std::make_unique<LuaScriptEngine>();
    engine->initialize(success, failure, logger);
    return engine;
  };

  lua_script_engine_queue_ = utils::ResourceQueue<LuaScriptEngine>::create(create_engine, max_concurrent_engines, std::nullopt, logger);
}

REGISTER_RESOURCE(LuaScriptExecutor, InternalResource);

}  // namespace org::apache::nifi::minifi::extensions::lua
