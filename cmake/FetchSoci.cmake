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

set(IODBC_PC "${Patch_EXECUTABLE}" -p1 -i "${CMAKE_SOURCE_DIR}/thirdparty/iODBC/GCC-15-needs-typedef-SQLRETURN-HPROC.patch")

FetchContent_Declare(iODBC
        URL  https://github.com/openlink/iODBC/archive/refs/tags/v3.52.16.tar.gz
        URL_HASH SHA256=a0cf0375b462f98c0081c2ceae5ef78276003e57cdf1eb86bd04508fb62a0660
        PATCH_COMMAND "${IODBC_PC}"
        OVERRIDE_FIND_PACKAGE
        SYSTEM
)

FetchContent_MakeAvailable(iODBC)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SOCI_SHARED OFF CACHE BOOL "" FORCE)
set(WITH_ODBC OFF CACHE BOOL "" FORCE)
set(WITH_BOOST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(soci
        URL  https://github.com/SOCI/soci/archive/refs/tags/v4.1.2.tar.gz
        URL_HASH SHA256=c0974067e57242f21d9a85677c5f6cc7848fba3cbd5ec58d76c95570a5a7a15b
        OVERRIDE_FIND_PACKAGE
        SYSTEM
)

FetchContent_MakeAvailable(soci)