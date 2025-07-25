# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

include(FetchContent)

FetchContent_Declare(json-schema-validator
    URL https://github.com/pboettch/json-schema-validator/archive/2.2.0.tar.gz
    URL_HASH SHA256=03897867bd757ecac1db7545babf0c6c128859655b496582a9cea4809c2260aa
    SYSTEM)

FetchContent_MakeAvailable(json-schema-validator)

target_compile_options(nlohmann_json_schema_validator PRIVATE -fsigned-char)
