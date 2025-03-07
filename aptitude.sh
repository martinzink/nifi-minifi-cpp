#!/bin/bash
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

verify_enable_platform(){
    feature="$1"
    verify_gcc_enable "$feature"
}
add_os_flags() {
    CC=${CC:-gcc}
    CXX=${CXX:-g++}
    export CC
    export CXX
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX"
}
bootstrap_cmake(){
    ## on Ubuntu install the latest CMake
    echo "Adding KitWare CMake apt repository..."
    sudo apt-get update && sudo apt-get install -y apt-transport-https ca-certificates gnupg software-properties-common wget
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo apt-key add -
    sudo apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -c --short) main" && sudo apt-get update
    sudo apt-get -y install cmake
}
bootstrap_compiler() {
    compiler_pkgs="gcc g++"
    # shellcheck disable=SC2086
    sudo apt-get -y install $compiler_pkgs
}
build_deps(){
    COMMAND="sudo apt-get -y install zlib1g-dev libssl-dev uuid uuid-dev perl libbz2-dev libcurl4-openssl-dev"

    export DEBIAN_FRONTEND=noninteractive
    INSTALLED=()
    sudo apt-get -y update
    for option in "${OPTIONS[@]}" ; do
        option_value="${!option}"
        if [ "$option_value" = "${TRUE}" ]; then
            # option is enabled
            FOUND_VALUE=""
            for cmake_opt in "${DEPENDENCIES[@]}" ; do
                KEY=${cmake_opt%%:*}
                VALUE=${cmake_opt#*:}
                if [ "$KEY" = "$option" ]; then
                    FOUND_VALUE="$VALUE"
                    if [ "$FOUND_VALUE" = "openssl" ]; then
                        INSTALLED+=("openssl")
                    elif [ "$FOUND_VALUE" = "bison" ]; then
                        INSTALLED+=("bison")
                    elif [ "$FOUND_VALUE" = "flex" ]; then
                        INSTALLED+=("flex")
                    elif [ "$FOUND_VALUE" = "automake" ]; then
                        INSTALLED+=("automake")
                    elif [ "$FOUND_VALUE" = "autoconf" ]; then
                        INSTALLED+=("autoconf")
                    elif [ "$FOUND_VALUE" = "libtool" ]; then
                        INSTALLED+=("libtool")
                    elif [ "$FOUND_VALUE" = "python" ]; then
                        INSTALLED+=("libpython3-dev")
                    elif [ "$FOUND_VALUE" = "libarchive" ]; then
                        INSTALLED+=("liblzma-dev")
                    fi
                fi
            done

        fi
    done

    for option in "${INSTALLED[@]}" ; do
        COMMAND="${COMMAND} $option"
    done

    echo "Ensuring you have all dependencies installed..."
    ${COMMAND}

}
