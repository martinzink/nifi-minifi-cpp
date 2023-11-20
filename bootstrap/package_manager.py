# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import platform
import subprocess
import sys
import re
from typing import Dict, Set

from distro import distro


def _query_yes_no(question: str, no_confirm: bool) -> bool:
    valid = {"yes": True, "y": True, "ye": True, "no": False, "n": False}

    if no_confirm:
        print("Running {} with noconfirm".format(question))
        return True
    while True:
        print("{} [y/n]".format(question))
        choice = input().lower()
        if choice in valid:
            return valid[choice]
        else:
            print("Please respond with 'yes' or 'no' " "(or 'y' or 'n').")


def _run_command_with_confirm(command: str, no_confirm: bool) -> bool:
    if _query_yes_no("Running {}".format(command), no_confirm):
        return os.system(command) == 0


class PackageManager(object):
    def __init__(self, no_confirm):
        self.no_confirm = no_confirm
        pass

    def install(self, dependencies: Dict[str, Set[str]]):
        raise Exception("NotImplementedException")

    def install_compiler(self):
        raise Exception("NotImplementedException")

    def _install(self, dependencies: Dict[str, Set[str]], replace_dict: Dict[str, Set[str]], install_cmd: str):
        print(f"Dependencies to install f{dependencies}")
        dependencies.update({k: v for k, v in replace_dict.items() if k in dependencies})
        print(f"after replace_dict f{dependencies}")
        dependencies = self._filter_out_installed_packages(dependencies)
        print(f"after filter f{dependencies}")
        dependencies_str = " ".join(str(value) for value_set in dependencies.values() for value in value_set)
        if not dependencies_str or dependencies_str.isspace():
            return
        _run_command_with_confirm(f"{install_cmd} {dependencies_str}", self.no_confirm)

    def _get_installed_packages(self) -> Set[str]:
        raise Exception("NotImplementedException")

    def _filter_out_installed_packages(self, dependencies: Dict[str, Set[str]]):
        installed_packages = self._get_installed_packages()
        return {k: (v - installed_packages) for k, v in dependencies.items()}

    def run_cmd(self, cmd: str) -> bool:
        return os.system(cmd) == 0


class BrewPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]):
        self._install(dependencies=dependencies,
                      install_cmd="brew install",
                      replace_dict={"patch": set(),
                                    "jni": {"maven"}})

    def install_compiler(self) -> str:
        self.install({"compiler": {"llvm"}})
        return ""

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['brew', 'list'], text=True, capture_output=True, check=True)
        lines = result.stdout.splitlines()
        lines = [line.split('@', 1)[0] for line in lines]
        return set(lines)


class AptPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]):
        self._install(dependencies=dependencies,
                      install_cmd="sudo apt install -y",
                      replace_dict={"libarchive": {"liblzma-dev"},
                                    "lua": {"liblua5.1-0-dev"},
                                    "python": {"libpython3-dev"},
                                    "libusb": {"libusb-1.0-0-dev", "libusb-dev"},
                                    "libpng": {"libpng-dev"},
                                    "libpcap": {"libpcap-dev"},
                                    "jni": {"openjdk-8-jdk", "openjdk-8-source", "maven"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['dpkg', '--get-selections'], text=True, capture_output=True, check=True)
        lines = [line.split('\t')[0] for line in result.stdout.splitlines()]
        lines = [line.rsplit(':', 1)[0] for line in lines]
        return set(lines)

    def install_compiler(self) -> str:
        if distro.id() == "ubuntu" and int(distro.major_version()) < 22:
            self.install({"compiler_prereq": {"software-properties-common"}})
            _run_command_with_confirm("sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test",
                                      no_confirm=self.no_confirm)
            self.install({"compiler": {"build-essential", "g++-11"}})
            return "-DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11"
        self.install({"compiler": {"g++"}})
        return ""


class DnfPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]):
        self._install(dependencies=dependencies,
                      install_cmd="sudo dnf --enablerepo=crb install -y epel-release",
                      replace_dict={"gpsd": {"gpsd-devel"},
                                    "libpcap": {"libpcap-devel"},
                                    "lua": {"lua-devel"},
                                    "python": {"python3-devel"},
                                    "jni": {"java-1.8.0-openjdk", "java-1.8.0-openjdk-devel", "maven"},
                                    "libpng": {"libpng-devel"},
                                    "libusb": {"libusb-devel"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['dnf', 'list', 'installed'], text=True, capture_output=True, check=True)
        lines = [line.split(' ')[0] for line in result.stdout.splitlines()]
        lines = [line.rsplit('.', 1)[0] for line in lines]
        return set(lines)

    def install_compiler(self) -> str:
        self.install({"compiler": {"gcc-c++"}})
        return ""


class PacmanPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]):
        self._install(dependencies=dependencies,
                      install_cmd="sudo pacman --noconfirm -S",
                      replace_dict={"jni": {"jdk8-openjdk", "maven"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['pacman', '-Qq'], text=True, capture_output=True, check=True)
        return set(result.stdout.splitlines())

    def install_compiler(self) -> str:
        self.install({"compiler": {"gcc"}})
        return ""


class WingetPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]):
        self._install(dependencies=dependencies,
                      install_cmd="winget install --disable-interactivity --accept-package-agreements",
                      replace_dict={"lua": {"DEVCOM.Lua"},
                                    "python": {"python"},
                                    "patch": set(),
                                    "bison": set(),
                                    "flex": set()})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['winget', 'list'], text=True, capture_output=True, check=True)
        separator_index = result.stdout.find("-----")
        result_set = set()

        for line in result.stdout[separator_index:].splitlines()[1:]:
            package_columns = re.split(r"\s{2,}", line)
            result_set.add(package_columns[0])  # name
            result_set.add(package_columns[1])  # id
        return result_set

    def install_compiler(self) -> str:
        self.install({"path_updater": {"WingetPathUpdater"}})
        self.install({"compiler": {"Microsoft.VisualStudio.2022.BuildTools --override \"quiet --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.VC.CoreBuildTools --add Microsoft.VisualStudio.Component.VC.CoreIde\""}})
        return ""


def get_env_without_strawberry_c_bin():
    env = os.environ.copy()
    path = env["PATH"]
    path = path.replace(r"c:\StrawberryPerl\c\bin", "")
    env["PATH"] = path


class WingetPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]):
        self._install(dependencies=dependencies,
                      install_cmd="winget install --disable-interactivity --accept-package-agreements",
                      replace_dict={"lua": {"DEVCOM.Lua"},
                                    "python": {"python"},
                                    "patch": set(),
                                    "bison": set(),
                                    "flex": set(),
                                    "openssl": {"StrawberryPerl.StrawberryPerl", "NASM.NASM"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['winget', 'list'], text=True, capture_output=True, check=True)
        separator_index = result.stdout.find("-----")
        result_set = set()

        for line in result.stdout[separator_index:].splitlines()[1:]:
            package_columns = re.split(r"\s{2,}", line)
            result_set.add(package_columns[0])  # name
            result_set.add(package_columns[1])  # id
        return result_set

    def install_compiler(self) -> str:
        self.install({"path_updater": {"WingetPathUpdater"}})
        self.install({"compiler": {'Microsoft.VisualStudio.2022.BuildTools --silent --override "--wait --quiet '
                                   '--add Microsoft.VisualStudio.Workload.VCTools '
                                   '--add Microsoft.VisualStudio.Component.VC.ATL --includeRecommended"'}})
        return ""

    def run_cmd(self, cmd: str) -> bool:
        vs_cmd = r'"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"'

        subprocess.run(f"{vs_cmd} & {cmd}", shell=True, env=get_env_without_strawberry_c_bin())
        return os.system(cmd) == 0


def get_package_manager(no_confirm: bool) -> PackageManager:
    platform_system = platform.system()
    if platform_system == "Darwin":
        return BrewPackageManager(no_confirm)
    elif platform_system == "Linux":
        distro_id = distro.id()
        if distro_id == "ubuntu":
            return AptPackageManager(no_confirm)
        elif "arch" in distro_id or "manjaro" in distro_id:
            return PacmanPackageManager(no_confirm)
        elif "rocky" in distro_id:
            return DnfPackageManager(no_confirm)
        else:
            sys.exit(f"Unsupported platform {distro_id} exiting")
    elif platform_system == "Windows":
        return WingetPackageManager(no_confirm)
    else:
        sys.exit(f"Unsupported platform {platform_system} exiting")
