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

function(get_catch2)
    if(MINIFI_CATCH2_SOURCE STREQUAL "CONAN")
        message("Using Conan to install Catch2")
        find_package(Catch2 REQUIRED)
        add_library(Catch2WithMain ALIAS Catch2::Catch2WithMain)
    elseif(MINIFI_CATCH2_SOURCE STREQUAL "BUILD")
        message("Using CMake to build Catch2 from source")
        include(Catch2)
    endif()
endfunction(get_catch2)
