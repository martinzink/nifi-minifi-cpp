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
include(CMakeDependentOption)

set(MINIFI_DEPENDENCIES "")

if (ENABLE_EXPRESSION_LANGUAGE)
    list(APPEND MINIFI_DEPENDENCIES "bison")
    list(APPEND MINIFI_DEPENDENCIES "flex")
endif()

if (ENABLE_LIBARCHIVE)
    list(APPEND MINIFI_DEPENDENCIES "libarchive")
endif()

if (USB_ENABLED)
    list(APPEND MINIFI_DEPENDENCIES "libusb")
    list(APPEND MINIFI_DEPENDENCIES "libpng")
endif()

if(ENABLE_PCAP)
    list(APPEND MINIFI_DEPENDENCIES "libpcap")
endif()

if (ENABLE_GPS)
    list(APPEND MINIFI_DEPENDENCIES "gpsd")
endif()

if (ENABLE_COAP)
    list(APPEND MINIFI_DEPENDENCIES "automake")
    list(APPEND MINIFI_DEPENDENCIES "autoconf")
    list(APPEND MINIFI_DEPENDENCIES "libtool")
endif()

if (MINIFI_OPENSSL)
    list(APPEND MINIFI_DEPENDENCIES "libssl")
endif()


set(MINIFI_DEPENDENCIES ${MINIFI_DEPENDENCIES} CACHE STRING "The required dependencies based on options provided")