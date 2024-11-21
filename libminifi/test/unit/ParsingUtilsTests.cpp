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
#include "unit/Catch.h"
#include "utils/ParsingUtils.h"

namespace org::apache::nifi::minifi::parsing::test {

TEST_CASE("Test Permissions Conversion", "[testPermissions]") {
  CHECK(0777u == parsePermissions("0777"));
  CHECK(0000u == parsePermissions("0000"));
  CHECK(0644u == parsePermissions("0644"));

  CHECK_FALSE(parsePermissions("0999"));
  CHECK_FALSE(parsePermissions("999"));
  CHECK_FALSE(parsePermissions("0644a"));
  CHECK_FALSE(parsePermissions("07777"));

  CHECK(0777u == parsePermissions("rwxrwxrwx"));
  CHECK(0000u == parsePermissions("---------"));
  CHECK(0764u == parsePermissions("rwxrw-r--"));
  CHECK(0444u == parsePermissions("r--r--r--"));

  CHECK_FALSE(parsePermissions("wxrwxrwxr"));
  CHECK_FALSE(parsePermissions("foobarfoo"));
  CHECK_FALSE(parsePermissions("foobar"));
}

}
