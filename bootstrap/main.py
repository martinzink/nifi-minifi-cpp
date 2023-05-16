from minifi_option import parse_minifi_options
from cmake_parser import create_cmake_cache
from system_dependency import SystemDependency
from cli import main_menu
import pathlib

if __name__ == '__main__':
    # For parsing CMake we need a working compiler
    SystemDependency.install_compiler()
    minifi_options = parse_minifi_options(pathlib.Path(__file__).parent.resolve() / '../cmake/MiNiFiOptions.cmake')

    main_menu(minifi_options)
