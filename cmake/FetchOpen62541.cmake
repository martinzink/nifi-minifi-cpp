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


set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/open62541/add_UA_ENABLE_INSTALL_option.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(open62541
        URL "https://github.com/open62541/open62541/archive/refs/tags/v1.4.13.tar.gz"
        URL_HASH "SHA256=491f8c526ecd6f2240f29cf3a00b0498587474b8f0ced1b074589f54533542aa"
        PATCH_COMMAND "${PC}"
        OVERRIDE_FIND_PACKAGE
        SYSTEM
        EXCLUDE_FROM_ALL
)

set(UA_ENABLE_ENCRYPTION ON CACHE BOOL "" FORCE)
set(UA_FORCE_WERROR OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_DEBUG_SANITIZER OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_ENCRYPTION_MBEDTLS ON CACHE BOOL "" FORCE)
set(UA_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

include(FetchMbedTLS)
find_package(mbedTLS REQUIRED)
set(MBEDTLS_INCLUDE_DIRS ${mbedtls_SOURCE_DIR}/include CACHE STRING "" FORCE)

FetchContent_MakeAvailable(open62541)
target_link_libraries(open62541 PRIVATE mbedtls)
