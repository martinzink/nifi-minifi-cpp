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

if (TARGET CURL::libcurl)
    set(CURL_FOUND TRUE)
    return()
endif ()

if (NOT CURL_ROOT_DIR)
    message(FATAL_ERROR "Strict bundled cURL requires CURL_ROOT_DIR to be passed to this CMake scope!")
endif ()

find_library(CURL_LIBRARY
        NAMES curl curl-d libcurl libcurl-d
        PATHS "${CURL_ROOT_DIR}/lib" "${CURL_ROOT_DIR}/lib64"
        NO_DEFAULT_PATH # Strictly prevent system fallback
)

set(CURL_INCLUDE_DIR "${CURL_ROOT_DIR}/include")

if (NOT CURL_LIBRARY OR NOT EXISTS "${CURL_INCLUDE_DIR}")
    message(FATAL_ERROR "Failed to locate bundled cURL components inside ${CURL_ROOT_DIR}")
endif ()

set(CURL_FOUND TRUE)
set(CURL_INCLUDE_DIRS "${CURL_INCLUDE_DIR}")
set(CURL_LIBRARIES "${CURL_LIBRARY}")

add_library(CURL::libcurl STATIC IMPORTED)
set_target_properties(CURL::libcurl PROPERTIES
        IMPORTED_LOCATION "${CURL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
)
target_compile_definitions(CURL::libcurl INTERFACE CURL_STATICLIB)
