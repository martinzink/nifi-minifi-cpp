# Licensed to the Apache Software Foundation (ASF) under one
#
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

add_library(libmock-minifi INTERFACE)
target_link_libraries(libmock-minifi INTERFACE minifi-cpp-extension-lib minifi-core-framework-common minifi-c-api)
target_link_options(libmock-minifi INTERFACE -undefined dynamic_lookup)
target_include_directories(libmock-minifi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
