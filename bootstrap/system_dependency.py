from __future__ import annotations

import os
import platform
import sys
from typing import Dict

import distro

from minifi_option import MinifiOptions


def _query_yes_no(question: str, no_confirm: bool) -> bool:
    valid = {"yes": True, "y": True, "ye": True, "no": False, "n": False}

    if no_confirm:
        return True
    while True:
        print("{} [y/n]".format(question))
        choice = input().lower()
        if choice in valid:
            return valid[choice]
        else:
            print("Please respond with 'yes' or 'no' " "(or 'y' or 'n').")


def _get_system_identifier():
    platform_system = platform.system()
    if platform_system == "Linux":
        return distro.id()
    return platform_system


def _replace_wholewords(target: str, replace_dict: Dict[str]):
    words = target.split()
    output_string = ' '.join(replace_dict.get(word, word) for word in words)
    return output_string


def _run_command_with_confirm(command: str, no_confirm: bool) -> bool:
    if _query_yes_no("Running {}".format(command), no_confirm):
        return os.system(command) == 0


def _install_with_brew(dependencies_str: str, no_confirm: bool):
    replace_dict = {"patch": "",
                    "jni": "maven"}
    command = "brew install {}".format(
        _replace_wholewords(dependencies_str, replace_dict))
    assert _run_command_with_confirm(command, no_confirm)


def _install_with_apt(dependencies_str: str, no_confirm: bool):
    replace_dict = {"libarchive": "liblzma-dev",
                    "lua": "liblua5.1-0-dev",
                    "python": "libpython3-dev",
                    "libusb": "libusb-1.0-0-dev libusb-dev",
                    "libpng": "libpng-dev",
                    "libpcap": "libpcap-dev",
                    "jni": "openjdk-8-jdk openjdk-8-source maven"}

    command = "sudo apt install -y {}".format(
        _replace_wholewords(dependencies_str, replace_dict))
    assert _run_command_with_confirm(command, no_confirm)


def _install_with_dnf(dependencies_str: str, no_confirm: bool):
    replace_dict = {"gpsd": "gpsd-devel",
                    "libpcap": "libpcap-devel",
                    "lua": "lua-devel",
                    "python": "python-devel",
                    "jni": "java-1.8.0-openjdk java-1.8.0-openjdk-devel maven",
                    "libpng": "libpng-devel",
                    "libusb": "libusb-devel"}

    command = "sudo dnf --enablerepo=crb install -y epel-release {}".format(
        _replace_wholewords(dependencies_str, replace_dict))
    assert _run_command_with_confirm(command, no_confirm)


def _install_with_pacman(dependencies_str: str, no_confirm: bool):
    replace_dict = {"g++": "gcc",
                    "jni": "jdk8-openjdk maven"}
    command = "sudo pacman --noconfirm -S {}".format(
        _replace_wholewords(dependencies_str, replace_dict))
    assert _run_command_with_confirm(command, no_confirm)


def _install_with_winget(dependencies_str: str, no_confirm: bool):
    replace_dict = {"lua": "DEVCOM.Lua",
                    "python": "python",
                    "patch": "",
                    "bison": "",
                    "flex": ""}
    command = "winget install --disable-interactivity --accept-package-agreements {}".format(
        _replace_wholewords(dependencies_str, replace_dict))
    assert _run_command_with_confirm(command, no_confirm)


def _install(dependencies_str: str, no_confirm: bool):
    platform_system = platform.system()
    if platform_system == "Darwin":
        _install_with_brew(dependencies_str, no_confirm)
    elif platform_system == "Linux":
        distro_id = distro.id()
        if distro_id == "ubuntu":
            _install_with_apt(dependencies_str, no_confirm)
        elif "arch" in distro_id or "manjaro" in distro_id:
            _install_with_pacman(dependencies_str, no_confirm)
        elif "rocky" in distro_id:
            _install_with_dnf(dependencies_str, no_confirm)
        else:
            sys.exit(f"Unsupported platform {distro_id} exiting")
    elif platform_system == "Windows":
        _install_with_winget(dependencies_str, no_confirm)
    else:
        sys.exit(f"Unsupported platform {platform_system} exiting")


def _create_system_dependencies(minifi_options: MinifiOptions) -> str:
    system_dependencies = {'patch': 'patch', 'make': 'make'}
    if minifi_options.is_enabled("ENABLE_EXPRESSION_LANGUAGE"):
        system_dependencies['bison'] = 'bison'
        system_dependencies['flex'] = 'flex'
    if minifi_options.is_enabled("ENABLE_LIBARCHIVE"):
        system_dependencies['libarchive'] = 'libarchive'
    if minifi_options.is_enabled("ENABLE_PCAP"):
        system_dependencies['libpcap'] = 'libpcap'
    if minifi_options.is_enabled("ENABLE_USB_CAMERA"):
        system_dependencies['libusb'] = 'libusb'
        system_dependencies['libpng'] = 'libpng'
    if minifi_options.is_enabled("ENABLE_GPS"):
        system_dependencies['gpsd'] = 'gpsd'
    if minifi_options.is_enabled("ENABLE_COAP"):
        system_dependencies['automake'] = 'automake'
        system_dependencies['autoconf'] = 'autoconf'
        system_dependencies['libtool'] = 'libtool'
    if minifi_options.is_enabled("ENABLE_LUA_SCRIPTING"):
        system_dependencies['lua'] = 'lua'
    if minifi_options.is_enabled("ENABLE_PYTHON_SCRIPTING"):
        system_dependencies['python'] = 'python'
    if minifi_options.is_enabled("MINIFI_OPENSSL"):
        system_dependencies['openssl'] = 'perl'
    if minifi_options.is_enabled("ENABLE_JNI"):
        system_dependencies['jni'] = ''
    return " ".join(str(value) for value in system_dependencies.values())


def install_required(minifi_options: MinifiOptions):
    _install(_create_system_dependencies(minifi_options), minifi_options.no_confirm)


def install_compiler(no_confirm: bool) -> str:
    print("For CMake to work we need a working compiler")
    platform_system = platform.system()
    if platform_system == "Darwin":
        _install("llvm", no_confirm)
    elif platform_system == "Windows":
        _install("msvc", no_confirm)
    else:
        if distro.id() == "ubuntu" and int(distro.major_version()) < 22:
            _install_with_apt("software-properties-common", no_confirm)
            os.system("sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test")
            _install_with_apt("build-essential g++-11", no_confirm)
            return "-DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11"
        else:
            _install("g++", no_confirm)
    return ""
