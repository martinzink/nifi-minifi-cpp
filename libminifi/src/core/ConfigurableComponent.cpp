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

#include <utility>
#include <string>
#include <map>
#include <vector>

#include "core/ConfigurableComponent.h"
#include "core/logging/LoggerConfiguration.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {

ConfigurableComponent::ConfigurableComponent()
    : accept_all_properties_(false),
      logger_(logging::LoggerFactory<ConfigurableComponent>::getLogger()) {
}

ConfigurableComponent::~ConfigurableComponent() = default;

const Property* ConfigurableComponent::findProperty(const std::string_view name) const {
  const auto& it = properties_.find(name);
  if (it != properties_.end()) {
    return &it->second;
  }
  return nullptr;
}

Property* ConfigurableComponent::findProperty(const std::string_view name) {
  const auto& const_self = *this;
  return const_cast<Property*>(const_self.findProperty(name));
}

bool ConfigurableComponent::getProperty(const std::string_view name, Property &prop) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  if (auto prop_ptr = findProperty(name)) {
    prop = *prop_ptr;
    return true;
  }
  return false;
}


bool ConfigurableComponent::setProperty(const std::string_view name, const std::string& value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  if (const auto prop_ptr = findProperty(name)) {
    const Property orig_property = *prop_ptr;
    Property& new_property = *prop_ptr;
    auto onExit = gsl::finally([&]{
      onPropertyModified(orig_property, new_property);
      logger_->log_debug("Component {} property name {} value {}", name, new_property.getName(), value);
    });
    new_property.setValue(std::move(value));
    return true;
  }
  if (accept_all_properties_) {
    static const std::string STAR_PROPERTIES = "Property";
    Property new_property(std::string{name}, STAR_PROPERTIES, value, false, { }, { });
    new_property.setTransient();
    new_property.setValue(std::move(value));
    properties_.insert(std::pair<std::string, Property>(name, new_property));
    return true;
  }
  logger_->log_debug("Component {} cannot be set to {}", name, value);
  return false;
}

bool ConfigurableComponent::updateProperty(const std::string_view name, const std::string &value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  if (const auto prop_ptr = findProperty(name)) {
    const Property orig_property = *prop_ptr;
    Property& new_property = *prop_ptr;
    auto onExit = gsl::finally([&] {
      onPropertyModified(orig_property, new_property);
      logger_->log_debug("Component {} property name {} value {}", name, new_property.getName(), value);
    });
    new_property.addValue(value);
    return true;
  } else {
    return false;
  }
}

bool ConfigurableComponent::updateProperty(const PropertyReference& property, std::string_view value) {
  return updateProperty(std::string{property.name}, std::string{value});
}


bool ConfigurableComponent::setProperty(const Property& prop, const std::string& value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  if (const auto prop_ptr = findProperty(prop.getName())) {
    const Property orig_property = *prop_ptr;
    Property& new_property = *prop_ptr;
    auto onExit = gsl::finally([&] {
      onPropertyModified(orig_property, new_property);
      if (prop_ptr->isSensitive()) {
        logger_->log_debug("sensitive property name {} value has changed", prop_ptr->getName());
      } else {
        logger_->log_debug("property name {} value {} and new value is {}", prop_ptr->getName(), value, new_property.getValue().to_string());
      }
    });
    new_property.setValue(value);
    return true;
  }

  if (accept_all_properties_) {
    Property new_property(prop);
    new_property.setTransient();
    new_property.setValue(value);
    properties_.insert(std::pair<std::string, Property>(prop.getName(), new_property));
    if (prop.isSensitive()) {
      logger_->log_debug("Adding transient sensitive property name {}", prop.getName());
    } else {
      logger_->log_debug("Adding transient property name {} value {} and new value is {}", prop.getName(), value, new_property.getValue().to_string());
    }
    return true;
  }
  // Should not attempt to update dynamic properties here since the return code
  // is relied upon by other classes to determine if the property exists.
  return false;
}

bool ConfigurableComponent::setProperty(const PropertyReference& property, std::string_view value) {
  return setProperty(std::string{property.name}, std::string{value});
}

bool ConfigurableComponent::setProperty(const Property& prop, PropertyValue &value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  if (const auto prop_ptr = findProperty(prop.getName())) {
    const Property orig_property = *prop_ptr;
    Property& new_property = *prop_ptr;
    auto onExit = gsl::finally([&] {
      onPropertyModified(orig_property, new_property);
      if (prop.isSensitive()) {
        logger_->log_debug("sensitive property name {} value has changed", prop.getName());
      } else {
        logger_->log_debug("property name {} value {} and new value is {}", prop.getName(), value.to_string(), new_property.getValue().to_string());
      }
    });
    new_property.setValue(value);
    return true;
  }
  if (accept_all_properties_) {
    Property new_property(prop);
    new_property.setTransient();
    new_property.setValue(value);
    properties_.insert(std::pair<std::string, Property>(prop.getName(), new_property));
    if (prop.isSensitive()) {
      logger_->log_debug("Adding transient sensitive property name {}", prop.getName());
    } else {
      logger_->log_debug("Adding transient property name {} value {} and new value is {}", prop.getName(), value.to_string(), new_property.getValue().to_string());
    }
    return true;
  }

  return false;
}

void ConfigurableComponent::setSupportedProperties(std::span<const core::PropertyReference> properties) {
  if (!canEdit()) {
    return;
  }

  std::lock_guard<std::mutex> lock(configuration_mutex_);

  properties_.clear();
  for (const auto& item : properties) {
    properties_.emplace(item.name, item);
  }
}

bool ConfigurableComponent::getDynamicProperty(const std::string_view name, std::string &value) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  auto &&it = dynamic_properties_.find(name);
  if (it != dynamic_properties_.end()) {
    const Property& item = it->second;
    if (item.getValue().getValue() == nullptr) {
      // empty property value
      if (item.getRequired()) {
        logger_->log_error("Component {} required dynamic property {} is empty", name, item.getName());
        throw std::runtime_error(fmt::format("Required dynamic property is empty: {}", item.getName()));
      }
      logger_->log_debug("Component {} dynamic property name {}, empty value", name, item.getName());
      return false;
    }
    value = item.getValue().to_string();
    logger_->log_debug("Component {} dynamic property name {} value {}", name, item.getName(), value);
    return true;
  } else {
    return false;
  }
}

bool ConfigurableComponent::getDynamicProperty(const std::string_view name, core::Property &item) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  auto &&it = dynamic_properties_.find(name);
  if (it != dynamic_properties_.end()) {
    item = it->second;
    if (item.getValue().getValue() == nullptr) {
      // empty property value
      if (item.getRequired()) {
        logger_->log_error("Component {} required dynamic property {} is empty", name, item.getName());
        throw std::runtime_error(fmt::format("Required dynamic property is empty: {}", item.getName()));
      }
      logger_->log_debug("Component {} dynamic property name {}, empty value", name, item.getName());
      return false;
    }
    return true;
  }
  return false;
}

bool ConfigurableComponent::createDynamicProperty(std::string name, const std::string& value) {
  if (!supportsDynamicProperties()) {
    logger_->log_debug(
        "Attempted to create dynamic property {}, but this component does not support creation."
        "of dynamic properties.",
        name);
    return false;
  }

  static const std::string DEFAULT_DYNAMIC_PROPERTY_DESC = "Dynamic Property";
  Property new_property(std::move(name), DEFAULT_DYNAMIC_PROPERTY_DESC, value, false, {}, {});
  new_property.setValue(value);
  new_property.setSupportsExpressionLanguage(true);
  logger_->log_info("Dynamic property '{}' value '{}'", new_property.getName(), value);
  dynamic_properties_[std::string{new_property.getName()}] = new_property;
  onDynamicPropertyModified({}, new_property);
  return true;
}

template<class T>
bool ConfigurableComponent::setTransientProperty(const Property& prop, T& value) {
  Property new_property(prop);
  new_property.setTransient();
  new_property.setValue(value);
  properties_.insert(std::pair<std::string, Property>(prop.getName(), new_property));
  if (prop.isSensitive()) {
    logger_->log_debug("Adding transient sensitive property name {}", prop.getName());
  } else {
    logger_->log_debug("Adding transient property name {} value {} and new value is {}", prop.getName(), value, new_property.getValue().to_string());
  }
  return true;
}

bool ConfigurableComponent::setDynamicProperty(const std::string_view name, const std::string& value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto &&it = dynamic_properties_.find(name);

  if (it != dynamic_properties_.end()) {
    const Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&] {
      onDynamicPropertyModified(orig_property, new_property);
      logger_->log_debug("Component {} dynamic property name {} value {}", name, new_property.getName(), value);
    });
    new_property.setValue(std::move(value));
    new_property.setSupportsExpressionLanguage(true);
    return true;
  } else {
    return createDynamicProperty(std::string{name}, std::move(value));
  }
}

bool ConfigurableComponent::updateDynamicProperty(const std::string_view name, const std::string &value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto &&it = dynamic_properties_.find(name);

  if (it != dynamic_properties_.end()) {
    const Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&] {
      onDynamicPropertyModified(orig_property, new_property);
      logger_->log_debug("Component {} dynamic property name {} value {}", name, new_property.getName(), value);
    });
    new_property.addValue(value);
    new_property.setSupportsExpressionLanguage(true);
    return true;
  }
  return createDynamicProperty(std::string{name}, value);
}

std::vector<std::string> ConfigurableComponent::getDynamicPropertyKeys() const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  return dynamic_properties_
      | ranges::views::transform([](const auto& kv) { return kv.first; })
      | ranges::to<std::vector>();
}

std::map<std::string, Property> ConfigurableComponent::getProperties() const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  std::map<std::string, Property> result;

  for (const auto &pair : properties_) {
    result.insert({ pair.first, pair.second });
  }

  for (const auto &pair : dynamic_properties_) {
    result.insert({ pair.first, pair.second });
  }

  return result;
}

bool ConfigurableComponent::isPropertyExplicitlySet(const Property& searched_prop) const {
  Property prop;
  return getProperty(searched_prop.getName(), prop) && !prop.getValues().empty();
}

bool ConfigurableComponent::isPropertyExplicitlySet(const PropertyReference& searched_prop) const {
  Property prop;
  return getProperty(std::string(searched_prop.name), prop) && !prop.getValues().empty();
}

std::optional<std::string> ConfigurableComponent::getPropertyString(const std::string_view name) const {
  if (const auto prop_ptr = findProperty(std::string{name})) {
    return prop_ptr->getValue().getValue()->getStringValue();
  }
  logger_->log_warn("Could not find property {}", name);
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::core
