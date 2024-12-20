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
  #CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND}"
  :
}
bootstrap_cmake(){
  if [ "$VERSION_CODENAME" = buster ]; then
    sudo bash -c 'source /etc/os-release; grep "$VERSION_CODENAME-backports" /etc/apt/sources.list &>/dev/null || echo "deb http://deb.debian.org/debian $VERSION_CODENAME-backports main" >> /etc/apt/sources.list'
    sudo apt-get -y update
    sudo apt-get -t buster-backports install -y cmake
  else
    sudo apt-get -y update
    sudo apt-get install -y cmake
  fi
}
bootstrap_compiler(){
  sudo apt-get -y install build-essential
}
build_deps(){
  sudo apt-get -y update
  RETVAL=$?
  if [ "$RETVAL" -ne "0" ]; then
    sudo apt-get install -y libssl-dev > /dev/null
  fi
  COMMAND="sudo apt-get -y install gcc g++ zlib1g-dev uuid uuid-dev perl libbz2-dev"
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
          if [ "$FOUND_VALUE" = "bison" ]; then
            INSTALLED+=("bison")
          elif [ "$FOUND_VALUE" = "flex" ]; then
            INSTALLED+=("flex")
          elif [ "$FOUND_VALUE" = "libtool" ]; then
            INSTALLED+=("libtool")
          elif [ "$FOUND_VALUE" = "python" ]; then
            INSTALLED+=("libpython3-dev")
          elif [ "$FOUND_VALUE" = "automake" ]; then
            INSTALLED+=("automake")
          elif [ "$FOUND_VALUE" = "libarchive" ]; then
            INSTALLED+=("liblzma-dev")
          elif [ "$FOUND_VALUE" = "libssh2" ]; then
            INSTALLED+=("libssh2-1-dev")
          fi
        fi
      done

    fi
  done

  INSTALLED+=("autoconf")

  for option in "${INSTALLED[@]}" ; do
    COMMAND="${COMMAND} $option"
  done

  echo "Ensuring you have all dependencies installed..."
  ${COMMAND}

}
