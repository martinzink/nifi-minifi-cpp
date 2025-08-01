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
include(FetchContent)
FetchContent_Declare(Uvc
        URL  https://github.com/libuvc/libuvc/archive/refs/tags/v0.0.7.tar.gz
        URL_HASH SHA256=7c6ba79723ad5d0ccdfbe6cadcfbd03f9f75b701d7ba96631eb1fd929a86ee72
        OVERRIDE_FIND_PACKAGE
        SYSTEM
)
FetchContent_MakeAvailable(Uvc)
