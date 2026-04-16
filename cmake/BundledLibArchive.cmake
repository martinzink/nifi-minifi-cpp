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

function(use_bundled_libarchive SOURCE_DIR BINARY_DIR)
    message(STATUS "Using bundled libarchive via FetchContent")

    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)
    if (ENABLE_LZMA)
        find_package(LibLZMA REQUIRED)
    endif()
    if (ENABLE_BZIP2)
        find_package(BZip2 REQUIRED)
    endif()

    include(FetchContent)

    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libarchive/libarchive.patch")

    FetchContent_Declare(
            libarchive
            URL "https://github.com/libarchive/libarchive/archive/refs/tags/v3.8.7.tar.gz"
            URL_HASH "SHA256=bc942030fe7cb30e04eed31bd5f63c38cdfd712315b303e91b64e58f05db2346"
            PATCH_COMMAND ${PC}
            SYSTEM
    )

    set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
    set(ENABLE_NETTLE OFF CACHE BOOL "" FORCE)
    set(ENABLE_LIBB2 OFF CACHE BOOL "" FORCE)
    set(ENABLE_LZ4 OFF CACHE BOOL "" FORCE)
    set(ENABLE_LZO OFF CACHE BOOL "" FORCE)
    set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
    set(ENABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(ENABLE_LIBXML2 OFF CACHE BOOL "" FORCE)
    set(ENABLE_EXPAT OFF CACHE BOOL "" FORCE)
    set(ENABLE_PCREPOSIX OFF CACHE BOOL "" FORCE)
    set(ENABLE_TAR OFF CACHE BOOL "" FORCE)
    set(ENABLE_CPIO OFF CACHE BOOL "" FORCE)
    set(ENABLE_CAT OFF CACHE BOOL "" FORCE)
    set(ENABLE_XATTR ON CACHE BOOL "" FORCE)
    set(ENABLE_ACL ON CACHE BOOL "" FORCE)
    set(ENABLE_ICONV OFF CACHE BOOL "" FORCE)
    set(ENABLE_TEST OFF CACHE BOOL "" FORCE)
    set(ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPENSSL ON CACHE BOOL "" FORCE)
    set(ENABLE_UNZIP OFF CACHE BOOL "" FORCE)

    if (ENABLE_LZMA)
        set(ENABLE_LZMA ON CACHE BOOL "" FORCE)
    else()
        set(ENABLE_LZMA OFF CACHE BOOL "" FORCE)
    endif()

    if (ENABLE_BZIP2)
        set(ENABLE_BZip2 ON CACHE BOOL "" FORCE)
    else()
        set(ENABLE_BZip2 OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_MakeAvailable(libarchive)

    add_library(LibArchive::LibArchive ALIAS archive_static)

    if (WIN32)
        target_link_libraries(archive_static PRIVATE xmllite)
    endif()

    set(LIBARCHIVE_FOUND "YES" CACHE INTERNAL "")
    set(LIBARCHIVE_INCLUDE_DIRS "${libarchive_SOURCE_DIR}/libarchive" CACHE INTERNAL "")
    set(LIBARCHIVE_LIBRARIES LibArchive::LibArchive CACHE INTERNAL "")
endfunction(use_bundled_libarchive)