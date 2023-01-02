/**
 *
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

#include "LuaScriptStateManager.h"

namespace org::apache::nifi::minifi::script {

namespace {
core::CoreComponentState from_lua(const sol::table& core_component_state_lua) {
  core::CoreComponentState core_component_state_cpp;
  for (const auto& [key, value] : core_component_state_lua)
    core_component_state_cpp[key.as<std::string>()] = value.as<std::string>();
  return core_component_state_cpp;
}

sol::table to_lua(const core::CoreComponentState& core_component_state_cpp, sol::state* sol_state) {
  auto core_component_state_lua = sol::table(sol_state->lua_state(), sol::create);
  for (const auto& [key, value] : core_component_state_cpp)
    core_component_state_lua[key] = value;
  return core_component_state_lua;
}
}  // namespace

bool LuaScriptStateManager::set(const sol::table& core_component_state_lua) {
  if (!state_manager_)
    return false;

  return state_manager_->set(from_lua(core_component_state_lua));
}

sol::optional<sol::table> LuaScriptStateManager::get() {
  if (!state_manager_)
    return sol::nullopt;
  if (auto core_component_state_cpp = state_manager_->get())
    return to_lua(*core_component_state_cpp, sol_state_);
  return sol::nullopt;
}

}  // namespace org::apache::nifi::minifi::script
