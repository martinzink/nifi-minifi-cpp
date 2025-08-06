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

set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/lz4/Update-CMake-version-requirements-to-support-broader.patch")
set(PC "${Patch_EXECUTABLE}" -p1 -i "${PATCH_FILE}")

FetchContent_Declare(lz4
    URL            https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
    URL_HASH       SHA256=537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b
    SOURCE_SUBDIR  build/cmake
    #PATCH_COMMAND  "${PC}"
    OVERRIDE_FIND_PACKAGE
    SYSTEM
)

FetchContent_MakeAvailable(lz4)

if(NOT TARGET LZ4::LZ4)
    add_library(LZ4::LZ4 ALIAS lz4_static)
endif()