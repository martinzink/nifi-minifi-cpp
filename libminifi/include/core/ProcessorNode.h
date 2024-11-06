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

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ConfigurableComponent.h"
#include "Connectable.h"
#include "Processor.h"

namespace org::apache::nifi::minifi::core {

/**
 * Processor node functions as a pass through to the implementing Connectables
 */
class ProcessorNode {
 public:
  explicit ProcessorNode(Processor& processor);

  ProcessorNode(const ProcessorNode &other) = delete;
  ProcessorNode(ProcessorNode &&other) = delete;

  ~ProcessorNode();

  ProcessorNode& operator=(const ProcessorNode &other) = delete;
  ProcessorNode& operator=(ProcessorNode &&other) = delete;

  [[nodiscard]] Processor& getProcessor() const {
    return processor_;
  }

  void yield() const { processor_.yield(); }

  template<typename T>
  bool getProperty(const std::string_view name, T &value) const {
    return processor_.getProperty<T>(name, value);
  }

  bool setProperty(const Property &prop, const std::string& value, const bool validate) {
    return processor_.setProperty(prop, value, validate);
  }

  bool setProperty(const std::string_view name, const std::string& value, const bool validate) {
    return processor_.setProperty(name, value, validate);
  }

  bool getDynamicProperty(const std::string_view name, std::string &value) const {
    return processor_.getDynamicProperty(name, value);
  }

  bool setDynamicProperty(const std::string_view name, std::string value) {
    return processor_.setDynamicProperty(name, std::move(value));
  }

  [[nodiscard]] std::vector<std::string> getDynamicPropertyKeys() const {
    return processor_.getDynamicPropertyKeys();
  }

  [[nodiscard]] bool isAutoTerminated(const Relationship& relationship) {
    return processor_.isAutoTerminated(relationship);
  }

  [[nodiscard]] std::optional<std::string> getPropertyString(const std::string_view name) const {
    return processor_.getPropertyString(name);
  }

  [[nodiscard]] std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const {
    return processor_.getFlowIdentifier();
  }

  [[nodiscard]] std::string getName() const {
    return processor_.getName();
  }

  std::chrono::milliseconds getPenalizationPeriod() const {
    return processor_.getPenalizationPeriod();
  }

  std::set<Connectable*> getOutGoingConnections(const std::string& relationship) const {
    return processor_.getOutGoingConnections(relationship);
  }

  uint8_t getMaxConcurrentTasks() const {
    return processor_.getMaxConcurrentTasks();
  }

  [[nodiscard]] utils::SmallString<36> getUUIDStr() const {
    return processor_.getUUIDStr();
  }

  [[nodiscard]] Connectable* pickIncomingConnection() const {
    return processor_.pickIncomingConnection();
  }

 private:
  Processor& processor_;
};

}  // namespace org::apache::nifi::minifi::core
