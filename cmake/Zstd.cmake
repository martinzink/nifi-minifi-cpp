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

set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)

set(PC "")
if (WIN32)
    set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/zstd/exclude_gcc_clang_compiler_options_from_windows.patch")
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${PATCH_FILE}")
endif()

FetchContent_Declare(zstd
    URL            https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz
    URL_HASH       SHA256=37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3
    PATCH_COMMAND  "${PC}"
    SOURCE_SUBDIR  build/cmake
    OVERRIDE_FIND_PACKAGE
    SYSTEM
)

FetchContent_MakeAvailable(zstd)

if(NOT TARGET ZSTD::ZSTD)
    add_library(ZSTD::ZSTD ALIAS libzstd_static)
    add_library(zstd::zstd ALIAS libzstd_static)
endif()

find_package(zstd REQUIRED)