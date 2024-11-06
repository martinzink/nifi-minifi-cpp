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

#include <string>
#include <vector>

#include "Core.h"
#include <mutex>
#include <iostream>
#include <map>
#include <memory>

#include "logging/Logger.h"
#include "Property.h"
#include "PropertyDefinition.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {

/**
 * Represents a configurable component
 * Purpose: Extracts configuration items for all components and localized them
 */
class ConfigurableComponent {
 public:
  ConfigurableComponent();

  ConfigurableComponent(const ConfigurableComponent &other) = delete;
  ConfigurableComponent(ConfigurableComponent &&other) = delete;

  ConfigurableComponent& operator=(const ConfigurableComponent &other) = delete;
  ConfigurableComponent& operator=(ConfigurableComponent &&other) = delete;


  template<typename T>
  bool getProperty(std::string_view name, T &value) const;

  template<typename T>
  bool getProperty(const core::PropertyReference& property, T& value) const { return getProperty(property.name, value); }

  template<typename T = std::string> requires(std::is_default_constructible_v<T>)
  std::optional<T> getProperty(std::string_view property_name) const;

  template<typename T = std::string> requires(std::is_default_constructible_v<T>)
  std::optional<T> getProperty(const core::PropertyReference& property) const { return getProperty<T>(property.name); }

  bool getProperty(std::string_view name, Property &prop) const;

  virtual std::optional<std::string> getPropertyString(std::string_view name) const;

  bool setProperty(std::string_view name, const std::string& value, bool validate = true);
  bool setProperty(const Property& prop, const std::string& value, bool validate = true);
  bool setProperty(const PropertyReference& property, std::string_view value, bool validate = true);
  bool setProperty(const Property& prop, PropertyValue &value, bool validate = true);


  bool updateProperty(std::string_view name, const std::string &value, bool validate = true);
  bool updateProperty(const PropertyReference& property, std::string_view value, bool validate = true);


  void setSupportedProperties(std::span<const core::PropertyReference> properties);

  /**
   * Gets whether or not this processor supports dynamic properties.
   *
   * @return true if this component supports dynamic properties (default is false)
   */
  virtual bool supportsDynamicProperties() const = 0;

  /**
   * Gets whether or not this processor supports dynamic relationships.
   *
   * @return true if this component supports dynamic relationships (default is false)
   */
  virtual bool supportsDynamicRelationships() const = 0;

  bool getDynamicProperty(std::string_view name, std::string &value) const;
  bool getDynamicProperty(std::string_view name, Property &item) const;
  bool setDynamicProperty(std::string_view name, const std::string& value);

  bool updateDynamicProperty(std::string_view name, const std::string &value);

  /**
   * Invoked anytime a static property is modified
   */
  virtual void onPropertyModified(const Property& /*old_property*/, const Property& /*new_property*/) {
  }

  /**
   * Invoked anytime a dynamic property is modified.
   */
  virtual void onDynamicPropertyModified(const Property& /*old_property*/, const Property& /*new_property*/) {
  }

  std::vector<std::string> getDynamicPropertyKeys() const;

  virtual std::map<std::string, Property> getProperties() const;

  bool isPropertyExplicitlySet(const Property&) const;
  bool isPropertyExplicitlySet(const PropertyReference&) const;

  virtual ~ConfigurableComponent();

  virtual void initialize() {
  }

 protected:
  void setAcceptAllProperties() {
    accept_all_properties_ = true;
  }

  /**
   * Returns true if the instance can be edited.
   * @return true/false
   */
  virtual bool canEdit() = 0;

  mutable std::mutex configuration_mutex_;

  bool accept_all_properties_;

  // Supported properties
  std::map<std::string, Property, std::less<>> properties_;

  // Dynamic properties
  std::map<std::string, Property, std::less<>> dynamic_properties_;

  virtual const Property* findProperty(std::string_view name) const;

 private:
  Property* findProperty(std::string_view name);
  std::shared_ptr<logging::Logger> logger_;

  bool createDynamicProperty(std::string name, const std::string &value);

  template<class T>
  bool setTransientProperty(const Property& prop, T& value);
};

template<typename T>
bool ConfigurableComponent::getProperty(const std::string_view name, T &value) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  if (const auto prop_ptr = findProperty(name)) {
    const Property& property = *prop_ptr;
    if (property.getValue().getValue() == nullptr) {
      // empty value
      if (property.getRequired()) {
        logger_->log_error("Component {} required property {} is empty", name, property.getName());
        throw utils::internal::RequiredPropertyMissingException(fmt::format("Required property is empty: {}", property.getName()));
      }
      logger_->log_debug("Component {} property name {}, empty value", name, property.getName());
      return false;
    }
    logger_->log_debug("Component {} property name {} value {}", name, property.getName(), property.getValue().to_string());

    if constexpr (std::is_base_of_v<TransformableValue, T>) {
      value = T(property.getValue().operator std::string());
    } else {
      value = static_cast<T>(property.getValue());  // cast throws if the value is invalid
    }
    return true;
  }
  logger_->log_warn("Could not find property {}", name);
  return false;
}

template<typename T> requires(std::is_default_constructible_v<T>)
std::optional<T> ConfigurableComponent::getProperty(const std::string_view property_name) const {
  T value;
  try {
    if (!getProperty(property_name, value)) { return std::nullopt; }
  } catch (const utils::internal::ValueException&) {
    return std::nullopt;
  }
  return value;
}

}  // namespace org::apache::nifi::minifi::core
