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

function(use_bundled_curl SOURCE_DIR BINARY_DIR)
    # Ensure our global OpenSSL targets are ready before building curl
    find_package(OpenSSL REQUIRED)

    # Define patch step
    set(PATCH_FILE_1 "${SOURCE_DIR}/thirdparty/curl/module-path.patch")
    set(PC ${Bash_EXECUTABLE} -c "set -x && \
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE_1}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE_1}\")")
    # Define byproducts
    string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
    if (build_type MATCHES "relwithdebinfo|release|minsizerel")
        set(CURL_LIB_NAME "curl")
    else ()
        set(CURL_LIB_NAME "curl-d")
    endif ()

    if (WIN32)
        set(CURL_LIBDIR "lib")
        set(BYPRODUCT "${CURL_LIBDIR}/lib${CURL_LIB_NAME}.lib")
    else ()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 CURL_LIBDIR)
        set(BYPRODUCT "${CURL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${CURL_LIB_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endif ()

    set(CURL_INSTALL_DIR "${BINARY_DIR}/thirdparty/curl-install")

    # Set build options
    set(CURL_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${CURL_INSTALL_DIR}"

            # --- THE BUNDLED OPENSSL HANDOFF ---
            # Give cURL access to our strict FindOpenSSL.cmake
            "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}"
            # Tell our FindOpenSSL.cmake exactly where to look
            "-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}"
            # -----------------------------------

            -DBUILD_CURL_EXE=OFF
            -DBUILD_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DHTTP_ONLY=ON
            -DCURL_CA_PATH=none
            -DCURL_USE_LIBSSH2=OFF
            -DUSE_LIBIDN2=OFF
            -DCURL_USE_LIBPSL=OFF
            -DCURL_USE_OPENSSL=ON
            -DUSE_NGHTTP2=OFF
            -DCURL_ZSTD=OFF
            -DCURL_BROTLI=OFF
    )

    append_third_party_passthrough_args(CURL_CMAKE_ARGS "${CURL_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            curl-external
            URL "https://github.com/curl/curl/releases/download/curl-8_18_0/curl-8.18.0.tar.gz"
            URL_HASH "SHA256=e9274a5f8ab5271c0e0e6762d2fce194d5f98acc568e4ce816845b2dcc0cf88f"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/curl-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${CURL_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${CURL_INSTALL_DIR}/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
    )

    # Set dependencies
    add_dependencies(curl-external ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto)

    # Reconstruct Paths
    set(CURL_INCLUDE_DIR "${CURL_INSTALL_DIR}/include")
    set(CURL_LIBRARY "${CURL_INSTALL_DIR}/${BYPRODUCT}")

    # Export Variables to Parent Scope
    set(CURL_FOUND "YES" PARENT_SCOPE)
    set(CURL_INCLUDE_DIR "${CURL_INCLUDE_DIR}" PARENT_SCOPE)
    set(CURL_INCLUDE_DIRS "${CURL_INCLUDE_DIR}" PARENT_SCOPE)
    set(CURL_LIBRARY "${CURL_LIBRARY}" PARENT_SCOPE)
    set(CURL_LIBRARIES "${CURL_LIBRARY}" PARENT_SCOPE)

    # Create imported target (made GLOBAL so other directories can see it)
    file(MAKE_DIRECTORY "${CURL_INCLUDE_DIR}")

    add_library(CURL::libcurl STATIC IMPORTED GLOBAL)
    set_target_properties(CURL::libcurl PROPERTIES IMPORTED_LOCATION "${CURL_LIBRARY}")
    add_dependencies(CURL::libcurl curl-external)

    target_include_directories(CURL::libcurl INTERFACE "${CURL_INCLUDE_DIR}")
    target_link_libraries(CURL::libcurl INTERFACE ZLIB::ZLIB Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(CURL::libcurl INTERFACE CURL_STATICLIB)

    if (APPLE)
        # Consolidated framework linking
        target_link_libraries(CURL::libcurl INTERFACE "-framework CoreFoundation -framework SystemConfiguration -framework CoreServices")
    elseif (WIN32)
        target_link_libraries(CURL::libcurl INTERFACE Iphlpapi.lib)
    endif ()

endfunction(use_bundled_curl)
