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

if(WIN32)
    find_package(ODBC REQUIRED)
else()
    # Build iODBC

    # Define byproducts
    set(IODBC_BYPRODUCT "lib/libiodbc.a")

    set(IODBC_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/iodbc-install/")

    set(IODBC_PC "${Patch_EXECUTABLE}" -p1 -i "${CMAKE_SOURCE_DIR}/thirdparty/iODBC/GCC-15-needs-typedef-SQLRETURN-HPROC.patch")
    # Build project
    ExternalProject_Add(
            iodbc-external
            URL "https://github.com/openlink/iODBC/archive/v3.52.14.tar.gz"
            URL_HASH "SHA256=896d7e16b283cf9a6f5b5f46e8e9549aef21a11935726b0170987cd4c59d16db"
            BUILD_IN_SOURCE true
            SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/iodbc-src"
            PATCH_COMMAND "${IODBC_PC}"
            BUILD_COMMAND make
            CMAKE_COMMAND ""
            UPDATE_COMMAND ""
            INSTALL_COMMAND make install
            CONFIGURE_COMMAND ./autogen.sh && ./configure --prefix=${IODBC_BYPRODUCT_DIR} --with-pic
            STEP_TARGETS build
            BUILD_BYPRODUCTS "${IODBC_BYPRODUCT_DIR}/${IODBC_BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(IODBC_FOUND "YES" CACHE STRING "" FORCE)
    set(IODBC_INCLUDE_DIRS "${IODBC_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
    set(IODBC_LIBRARIES "${IODBC_BYPRODUCT_DIR}/${IODBC_BYPRODUCT}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(EXPORTED_IODBC_INCLUDE_DIRS "${IODBC_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(EXPORTED_IODBC_LIBRARIES "${IODBC_LIBRARIES}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(ODBC::ODBC STATIC IMPORTED)
    set_target_properties(ODBC::ODBC PROPERTIES IMPORTED_LOCATION "${IODBC_LIBRARIES}")
    add_dependencies(ODBC::ODBC iodbc-external)
    file(MAKE_DIRECTORY ${IODBC_INCLUDE_DIRS})
    set_property(TARGET ODBC::ODBC APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${IODBC_INCLUDE_DIRS})
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SOCI_SHARED OFF CACHE BOOL "" FORCE)
set(SOCI_STATIC ON CACHE BOOL "" FORCE)
set(SOCI_ODBC ON CACHE BOOL "" FORCE)
set(WITH_BOOST OFF CACHE BOOL "" FORCE)

set(ODBC_INCLUDE_DIR ${IODBC_INCLUDE_DIRS} CACHE STRING "" FORCE)
set(ODBC_LIBRARY ${IODBC_LIBRARIES} CACHE STRING "" FORCE)

FetchContent_Declare(soci
        URL  https://github.com/SOCI/soci/archive/refs/tags/v4.1.2.tar.gz
        URL_HASH SHA256=c0974067e57242f21d9a85677c5f6cc7848fba3cbd5ec58d76c95570a5a7a15b
        OVERRIDE_FIND_PACKAGE
        SYSTEM
        EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(soci)

