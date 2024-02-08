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

#pragma once
#include <numbers>

#include "core/Record.h"

namespace org::apache::nifi::minifi::core::test {

inline Record createSampleRecord2(bool stringify_date = false) {
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  Record record;

  auto when = date::sys_days(2022_y / 11 / 01) + 19h + 52min + 11s;
  if (!stringify_date) {
    record["when"] = RecordField{.value_ = when};
  } else {
    record["when"] = RecordField{.value_ = utils::timeutils::getDateTimeStr(std::chrono::floor<std::chrono::seconds>(when))};
  }
  record["foo"] = RecordField{.value_ = "Lorem ipsum dolor sit amet, consectetur adipiscing elit."};
  record["bar"] = RecordField{.value_ = int64_t{98402134}};
  record["baz"] = RecordField{.value_ = std::numbers::pi};
  record["is_test"] = RecordField{.value_ = true};
  RecordArray qux;
  qux.push_back(RecordField{.value_ = false});
  qux.push_back(RecordField{.value_ = false});
  qux.push_back(RecordField{.value_ = true});
  RecordObject quux;
  quux["Apfel"] = RecordField{.value_ = "pomme"};
  quux["Birne"] = RecordField{.value_ = "poire"};
  quux["Aprikose"] = RecordField{.value_ = "abricot"};

  record["qux"] = RecordField{.value_ = qux};
  record["quux"] = RecordField{.value_ = quux};
  return record;
}

inline Record createSampleRecord(bool stringify_date = false) {
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  Record record;

  auto when = date::sys_days(2012_y / 07 / 01) + 9h + 53min + 00s;
  if (!stringify_date) {
    record["when"] = RecordField{.value_ = when};
  } else {
    record["when"] = RecordField{.value_ = utils::timeutils::getDateTimeStr(std::chrono::floor<std::chrono::seconds>(when))};
  }
  record["foo"] = RecordField{.value_ = "asd"};
  record["bar"] = RecordField{.value_ = int64_t{123}};
  record["baz"] = RecordField{.value_ = 3.14};
  record["is_test"] = RecordField{.value_ = true};
  RecordArray qux;
  qux.push_back(RecordField{.value_ = true});
  qux.push_back(RecordField{.value_ = false});
  qux.push_back(RecordField{.value_ = true});
  RecordObject quux;
  quux["Apfel"] = RecordField{.value_ = "apple"};
  quux["Birne"] = RecordField{.value_ = "pear"};
  quux["Aprikose"] = RecordField{.value_ = "apricot"};

  record["qux"] = RecordField{.value_ = qux};
  record["quux"] = RecordField{.value_ = quux};
  return record;
}

}  // namespace org::apache::nifi::minifi::core::test
