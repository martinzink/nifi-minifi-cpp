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

if (NOT (ENABLE_ALL OR ENABLE_SYSTEMD) OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

add_minifi_library(minifi-systemd SHARED "ConsumeJournald.cpp" "libwrapper/LibWrapper.cpp" "libwrapper/DlopenWrapper.cpp")

target_link_libraries(minifi-systemd ${LIBMINIFI} Threads::Threads)

register_extension(minifi-systemd "SYSTEMD EXTENSIONS" SYSTEMD-EXTENSIONS "Enabled log collection from journald" "extensions/systemd/tests")
register_extension_linter(minifi-systemd-extension-linter)
