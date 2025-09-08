#
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
#

if (NOT (ENABLE_ALL OR ENABLE_SQL))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include_directories(SYSTEM ../../thirdparty/rapidjson-1.1.0/include/ ../../thirdparty/rapidjson-1.1.0/include/rapidjson)
include_directories(".")

file(GLOB SOURCES  "*.cpp" "services/*.cpp" "processors/*.cpp"  "data/*.cpp")

add_minifi_library(minifi-sql SHARED ${SOURCES})

include(FetchSoci)

target_link_libraries(minifi-sql soci_core soci_odbc)
target_link_libraries(minifi-sql ${LIBMINIFI} Threads::Threads)

register_extension(minifi-sql "SQL EXTENSIONS" SQL-EXTENSIONS "Enables the SQL Suite of Tools" "extensions/sql/tests")
