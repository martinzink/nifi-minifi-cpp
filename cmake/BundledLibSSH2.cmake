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

function(use_bundled_libssh2 SOURCE_DIR BINARY_DIR)
    message(STATUS "Using bundled libssh2")

    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)

    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libssh2/libssh2-CMAKE_MODULE_PATH.patch")

    if (WIN32)
        set(LIBSSH2_LIBDIR "lib")
        set(BYPRODUCT "${LIBSSH2_LIBDIR}/libssh2.lib")
    else ()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBSSH2_LIBDIR)
        set(BYPRODUCT "${LIBSSH2_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}ssh2${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endif ()

    set(LIBSSH2_INSTALL_DIR "${BINARY_DIR}/thirdparty/libssh2-install")

    string(REPLACE ";" "%" ESCAPED_CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}")

    set(LIBSSH2_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${LIBSSH2_INSTALL_DIR}"
            "-DCMAKE_MODULE_PATH=${ESCAPED_CMAKE_MODULE_PATH}"
            "-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}"
            -DENABLE_ZLIB_COMPRESSION=ON
            -DCRYPTO_BACKEND=OpenSSL
            -DBUILD_TESTING=OFF
            -DBUILD_EXAMPLES=OFF
    )

    ExternalProject_Add(
            libssh2-external
            URL "https://github.com/libssh2/libssh2/releases/download/libssh2-1.10.0/libssh2-1.10.0.tar.gz"
            URL_HASH "SHA256=2d64e90f3ded394b91d3a2e774ca203a4179f69aebee03003e5a6fa621e41d51"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/libssh2-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${LIBSSH2_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${LIBSSH2_INSTALL_DIR}/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
    )

    add_dependencies(libssh2-external OpenSSL::Crypto ZLIB::ZLIB)

    set(LIBSSH2_INCLUDE_DIR "${LIBSSH2_INSTALL_DIR}/include")
    set(LIBSSH2_LIBRARY "${LIBSSH2_INSTALL_DIR}/${BYPRODUCT}")

    set(LIBSSH2_ROOT_DIR "${LIBSSH2_INSTALL_DIR}" CACHE INTERNAL "Strict single source of truth for bundled libssh2")
    set(LIBSSH2_FOUND "YES" CACHE INTERNAL "")
    set(LIBSSH2_INCLUDE_DIR "${LIBSSH2_INCLUDE_DIR}" CACHE INTERNAL "")
    set(LIBSSH2_LIBRARY "${LIBSSH2_LIBRARY}" CACHE INTERNAL "")

    file(MAKE_DIRECTORY "${LIBSSH2_INCLUDE_DIR}")

    add_library(libssh2 STATIC IMPORTED GLOBAL)
    set_target_properties(libssh2 PROPERTIES IMPORTED_LOCATION "${LIBSSH2_LIBRARY}")
    add_dependencies(libssh2 libssh2-external)

    set_property(TARGET libssh2 APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto ZLIB::ZLIB)
    set_property(TARGET libssh2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LIBSSH2_INCLUDE_DIR}")
endfunction(use_bundled_libssh2)
