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

include(FetchZlib)
if (NOT WIN32)
    include(Zstd)
    include(LZ4)
endif()

set(PATCH_FILE_1 "${SOURCE_DIR}/thirdparty/rocksdb/all/patches/dboptions_equality_operator.patch")
set(PATCH_FILE_2 "${SOURCE_DIR}/thirdparty/rocksdb/all/patches/c++23_fixes.patch")

set(PC ${Bash_EXECUTABLE} -c "set -x &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE_1}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE_1}\") &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE_2}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE_2}\") ")


FetchContent_Declare(rocksdb
        URL "https://github.com/facebook/rocksdb/archive/refs/tags/v10.5.1.tar.gz"
        URL_HASH "SHA256=7ec942baab802b2845188d02bc5d4e42c29236e61bcbc08f5b3a6bdd92290c22"
        SYSTEM
)


set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(WITH_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_TOOLS OFF CACHE BOOL "" FORCE)
set(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "" FORCE)
set(WITH_GFLAGS OFF CACHE BOOL "" FORCE)
set(USE_RTTI ON CACHE BOOL "" FORCE)
set(ROCKSDB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(FAIL_ON_WARNINGS OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB ON CACHE BOOL "" FORCE)
set(WITH_BZ2 ON CACHE BOOL "" FORCE)
set(WITH_ZSTD ON CACHE BOOL "" FORCE)
set(WITH_LZ4 ON CACHE BOOL "" FORCE)
set(WITH_SNAPPY OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(rocksdb)
