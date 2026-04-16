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

function(use_openssl SOURCE_DIR BINARY_DIR)
    message(STATUS "Using bundled OpenSSL")

    # 1. Platform-specific library directory
    if (APPLE OR WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 4 OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|armv8)$")
        set(LIBDIR "lib")
    else ()
        set(LIBDIR "lib64")
    endif ()

    # 2. Native CMake library extensions
    set(OPENSSL_BUILD_SHARED OFF)
    set(LIB_PREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
    set(LIB_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})

    if (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
        set(OPENSSL_BUILD_SHARED ON)
        set(LIB_PREFIX ${CMAKE_SHARED_LIBRARY_PREFIX})
        set(LIB_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif ()

    set(OPENSSL_BIN_DIR "${BINARY_DIR}/thirdparty/openssl-install")
    set(OPENSSL_CRYPTO_LIB "${OPENSSL_BIN_DIR}/${LIBDIR}/${LIB_PREFIX}crypto${LIB_SUFFIX}")
    set(OPENSSL_SSL_LIB "${OPENSSL_BIN_DIR}/${LIBDIR}/${LIB_PREFIX}ssl${LIB_SUFFIX}")

    if (OPENSSL_BUILD_SHARED)
        set(OPENSSL_SHARED_FLAG "")
        install(FILES ${OPENSSL_CRYPTO_LIB} ${OPENSSL_SSL_LIB} DESTINATION bin COMPONENT bin)
    else ()
        set(OPENSSL_SHARED_FLAG "no-shared")
    endif ()

    set(OPENSSL_EXTRA_FLAGS no-tests no-capieng no-docs no-legacy enable-tfo no-ssl no-engine)
    set(OPENSSL_VERSION "3.3.6")

    # 3. Build Commands
    if (WIN32)
        find_program(JOM_EXECUTABLE NAMES jom.exe PATHS ENV PATH NO_DEFAULT_PATH)
        if (JOM_EXECUTABLE)
            include(ProcessorCount)
            ProcessorCount(jobs)
            set(OPENSSL_BUILD_COMMAND ${JOM_EXECUTABLE} -j${jobs})
            set(OPENSSL_WINDOWS_COMPILE_FLAGS "/FS")
        else ()
            set(OPENSSL_BUILD_COMMAND nmake)
            set(OPENSSL_WINDOWS_COMPILE_FLAGS "")
        endif ()

        set(OSSL_CONFIGURE perl Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
                "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}"
                "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}")
        set(OSSL_INSTALL nmake install)
    else ()
        set(OSSL_CONFIGURE ./Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
                "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} -fPIC"
                "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} -fPIC")
        set(OSSL_BUILD_COMMAND make)
        set(OSSL_INSTALL make install)
    endif ()

    # 4. Main OpenSSL ExternalProject
    ExternalProject_Add(
            openssl-external
            URL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
            URL_HASH "SHA256=22db04f3c8f9a808c9795dcf7d2713ff40c12c410ea2d1f6435c6c9c8558958b"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-src"
            BUILD_IN_SOURCE TRUE
            CONFIGURE_COMMAND ${OSSL_CONFIGURE} ${OPENSSL_SHARED_FLAG} ${OPENSSL_EXTRA_FLAGS} "--prefix=${OPENSSL_BIN_DIR}" "--openssldir=${OPENSSL_BIN_DIR}"
            BUILD_BYPRODUCTS "${OPENSSL_CRYPTO_LIB}" "${OPENSSL_SSL_LIB}"
            EXCLUDE_FROM_ALL TRUE
            BUILD_COMMAND ${OPENSSL_BUILD_COMMAND}
            INSTALL_COMMAND ${OSSL_INSTALL}
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
    )

    # 5. Create GLOBAL Imported Targets
    file(MAKE_DIRECTORY "${OPENSSL_BIN_DIR}/include")

    add_library(OpenSSL::Crypto UNKNOWN IMPORTED GLOBAL)
    set_target_properties(OpenSSL::Crypto PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_BIN_DIR}/include"
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIB}")
    add_dependencies(OpenSSL::Crypto openssl-external)

    add_library(OpenSSL::SSL UNKNOWN IMPORTED GLOBAL)
    set_target_properties(OpenSSL::SSL PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_BIN_DIR}/include"
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${OPENSSL_SSL_LIB}")
    add_dependencies(OpenSSL::SSL openssl-external)

    set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)

    if (WIN32)
        set_property(TARGET OpenSSL::Crypto APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib)
        set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib)
    endif ()

    # 6. FIPS Build - Streamlined
    set(FIPS_MODULE "${OPENSSL_BIN_DIR}/thirdparty/openssl-fips-install/${LIBDIR}/ossl-modules/fips${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(OPENSSL_FIPS_BIN_DIR "${BINARY_DIR}/thirdparty/openssl-fips-install")
    set(OPENSSL_FIPS_EXTRA_FLAGS no-tests no-capieng no-legacy no-ssl no-engine enable-fips)

    if (WIN32)
        set(FIPS_INSTALL_CMD nmake install_fips)
    else ()
        set(FIPS_INSTALL_CMD make install_fips)
    endif ()

    ExternalProject_Add(
            openssl-fips-external
            URL https://github.com/openssl/openssl/releases/download/openssl-3.0.9/openssl-3.0.9.tar.gz
            URL_HASH "SHA256=eb1ab04781474360f77c318ab89d8c5a03abc38e63d65a603cabbf1b00a1dc90"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-fips-src"
            BUILD_IN_SOURCE TRUE
            CONFIGURE_COMMAND ${OSSL_CONFIGURE} ${OPENSSL_SHARED_FLAG} ${OPENSSL_FIPS_EXTRA_FLAGS} "--prefix=${OPENSSL_FIPS_BIN_DIR}" "--openssldir=${OPENSSL_FIPS_BIN_DIR}"
            BUILD_BYPRODUCTS "${FIPS_MODULE}"
            EXCLUDE_FROM_ALL TRUE
            BUILD_COMMAND ${OPENSSL_BUILD_COMMAND}
            INSTALL_COMMAND ${FIPS_INSTALL_CMD}
    )
    add_dependencies(OpenSSL::Crypto openssl-fips-external)

    # 7. Packaging setup
    if (MINIFI_PACKAGING_TYPE MATCHES "^(RPM|TGZ)$")
        set(FIPS_DEST "${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}/fips")
        if (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
            set(FIPS_DEST "fips")
        endif ()

        install(FILES "${FIPS_MODULE}" DESTINATION "${FIPS_DEST}" COMPONENT bin)
        install(FILES "${OPENSSL_BIN_DIR}/bin/openssl${CMAKE_EXECUTABLE_SUFFIX}"
                DESTINATION "${FIPS_DEST}" COMPONENT bin
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE)
    endif ()

    # 8. EXPORT ONLY THE ROOT DIR TO PARENT SCOPE
    # This is the single source of truth for the strict FindOpenSSL.cmake
    set(OPENSSL_ROOT_DIR "${OPENSSL_BIN_DIR}" CACHE INTERNAL "Strict single source of truth for bundled OpenSSL")
endfunction(use_openssl)
