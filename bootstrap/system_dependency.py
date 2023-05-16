from __future__ import annotations

from typing import Callable

import platform
import os
import distro
import sys
from minifi_option import MinifiOptions


class SystemDependency:
    instances = []

    def __init__(self, name: str, is_required_fn: Callable[[MinifiOptions], bool] = None, depends_on_option: str = None):
        if depends_on_option is not None:
            self.is_required_fn = lambda minifi_option: minifi_option.is_enabled(depends_on_option)
        else:
            self.is_required_fn = is_required_fn
        self.name = name
        SystemDependency.instances.append(self)

    @staticmethod
    def install_with_brew(dependencies_str: str):
        command = "brew install {}".format(dependencies_str)
        print("Running {}".format(command))
        os.system(command)

    @staticmethod
    def install_with_apt(dependencies_str: str):
        command = "sudo apt install -y {}".format(dependencies_str)
        print("Running {}".format(command))
        os.system(command)

    @staticmethod
    def install_with_dnf(dependencies_str: str):
        command = "sudo dnf install -y {}".format(dependencies_str)
        print("Running {}".format(command))
        os.system(command)

    @staticmethod
    def install_with_pacman(dependencies_str: str):
        command = "sudo pacman --noconfirm -S {}".format(dependencies_str)
        print("Running {}".format(command))
        os.system(command)

    @staticmethod
    def install_with_winget(dependencies_str: str):
        command = "winget install -y {}".format(dependencies_str)
        print("Running {}".format(command))
        os.system(command)

    @staticmethod
    def install(dependencies_str: str):
        print("\nInstalling dependencies: {}".format(dependencies_str))
        platform_system = platform.system()
        if platform_system == "Darwin":
            SystemDependency.install_with_brew(dependencies_str)
        elif platform_system == "Linux":
            print(dir(distro))
            distro_id = distro.id()
            if distro_id == "Ubuntu":
                SystemDependency.install_with_apt(dependencies_str)
            elif "arch" in distro_id or "manjaro" in distro_id:
                SystemDependency.install_with_pacman(dependencies_str)
            else:
                sys.exit(f"Unsupported platform {platform.linux_distribution} exiting")
        else:
            sys.exit(f"Unsupported platform {platform_system} exiting")

    @staticmethod
    def install_required(minifi_options: MinifiOptions):
        dependencies_str = " ".join(dependency.name for dependency in SystemDependency.instances if dependency.is_required_fn(minifi_options))
        SystemDependency.install(dependencies_str)

    @staticmethod
    def install_compiler():
        platform_system = platform.system()
        if platform_system == "Darwin":
            SystemDependency.install("clang")
        elif platform_system == "Windows":
            SystemDependency.install("msvc")
        else:
            SystemDependency.install("g++")

    @staticmethod
    def name_to_platform_specific_name(name: str):
        return name


SystemDependency(name="bison", depends_on_option="ENABLE_EXPRESSION_LANGUAGE")
SystemDependency(name="flex", depends_on_option="ENABLE_EXPRESSION_LANGUAGE")

SystemDependency(name="libarchive", depends_on_option="ENABLE_LIBARCHIVE")

SystemDependency(name="libpcap", depends_on_option="ENABLE_GPS")

SystemDependency(name="libusb", depends_on_option="ENABLE_USB_CAMERA")
SystemDependency(name="libpng", depends_on_option="ENABLE_USB_CAMERA")

SystemDependency(name="gpsd", depends_on_option="ENABLE_GPS")

SystemDependency(name="automake", depends_on_option="ENABLE_COAP")
SystemDependency(name="autoconf", depends_on_option="ENABLE_COAP")
SystemDependency(name="libtool", depends_on_option="ENABLE_COAP")

SystemDependency(name="libssh2", depends_on_option="ENABLE_SFTP")

SystemDependency(name="boost", depends_on_option="ENABLE_BUSTACHE")
SystemDependency(name="mbedtls", depends_on_option="ENABLE_OPC")
