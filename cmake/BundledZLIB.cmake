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

function(use_bundled_zlib SOURCE_DIR BINARY_DIR)
    message(STATUS "Using bundled zlib via FetchContent")

    include(FetchContent)

    set(ZLIB_BUILD_TESTING OFF)
    set(ZLIB_BUILD_SHARED OFF)
    set(ZLIB_BUILD_STATIC ON)
    set(ZLIB_INSTALL OFF)

    FetchContent_Declare(
            ZLIB
            URL "https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz"
            URL_HASH "SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
            SYSTEM
            OVERRIDE_FIND_PACKAGE
    )

    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(ZLIB)

    add_library(ZLIB::ZLIB ALIAS zlibstatic)

    # --- EXPORT LEGACY VARIABLES ---
    set(ZLIB_FOUND "YES" CACHE INTERNAL "Short-circuits FindZLIB.cmake")
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE INTERNAL "")
    set(ZLIB_LIBRARIES ZLIB::ZLIB CACHE INTERNAL "")
endfunction(use_bundled_zlib)