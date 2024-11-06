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

#include "core/PropertyValue.h"
#include "core/PropertyType.h"

namespace org::apache::nifi::minifi::core {

PropertyValue::PropertyValue()
    : type_id(std::type_index(typeid(std::string))),
      validation_state_(ValidationState::RECOMPUTE),
      validator_(&StandardPropertyTypes::VALID_TYPE) {}

ValidationResult PropertyValue::validate(const std::string &subject) const {
  if (validation_state_ == ValidationState::VALID) {
    return ValidationResult{.valid = true, .subject = {}, .input = getValue()->getStringValue()};
  }
  if (validation_state_ == ValidationState::INVALID) {
    return ValidationResult{.valid = false, .subject = {}, .input = getValue()->getStringValue()};
  }
  return validator_->validate(subject, getValue());
}

}  // namespace org::apache::nifi::minifi::core
