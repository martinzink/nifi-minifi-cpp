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

function(use_bundled_libaws)
    # --- Step 1: Set the AWS SDK's options before fetching ---
    # This controls how the dependency will build itself.
    set(BUILD_ONLY "s3;kinesis" CACHE STRING "" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ENABLE_UNITY_BUILD ON CACHE BOOL "" FORCE)
    # This prevents the dependency from trying to install itself,
    # which is critical for our RPM packaging.
    set(CMAKE_INSTALL_PREFIX "" CACHE STRING "Disable install step" FORCE)


    # --- Step 2: Declare and fetch the content ---
    # We no longer need a PATCH_COMMAND if we configure it correctly.
    FetchContent_Declare(aws-sdk-cpp
            GIT_REPOSITORY "https://github.com/aws/aws-sdk-cpp.git"
            GIT_TAG "1.11.530"
    )
    FetchContent_MakeAvailable(aws-sdk-cpp)

    add_library(minifi_aws_sdk INTERFACE)

    # Link the real AWS targets that FetchContent made available
    target_link_libraries(minifi_aws_sdk
            INTERFACE
            aws-cpp-sdk-s3
            aws-cpp-sdk-kinesis
            aws-checksums
    )

    target_include_directories(minifi_aws_sdk
            INTERFACE
            "${aws-sdk-cpp_SOURCE_DIR}/src/aws-cpp-sdk-core/include/"
            "${aws-sdk-cpp_SOURCE_DIR}/crt/aws-crt-cpp/include/"
            "${aws-sdk-cpp_SOURCE_DIR}/crt/aws-crt-cpp/crt/aws-c-io/include/"
            "${aws-sdk-cpp_SOURCE_DIR}/crt/aws-crt-cpp/crt/aws-c-mqtt/include/"
            "${aws-sdk-cpp_SOURCE_DIR}/crt/aws-crt-cpp/crt/aws-c-auth/include/"
            "${aws-sdk-cpp_SOURCE_DIR}/crt/aws-crt-cpp/crt/aws-c-sdkutils/include/"
    )

    get_target_property(S3_INCLUDES minifi_aws_sdk INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "DEBUG S3 INCLUDES: ${S3_INCLUDES}")

    # Now, create the namespaced ALIAS pointing to our interface library
    add_library(AWS::SDK ALIAS minifi_aws_sdk)

endfunction()