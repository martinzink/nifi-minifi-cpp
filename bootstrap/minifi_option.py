from typing import Dict

import pathlib

import cmake_parser
from cmake_parser import CMakeCacheValue


class MinifiOptions:
    def __init__(self, cache_values: Dict[str, CMakeCacheValue]):
        self.build_type = CMakeCacheValue("Specifies the build type on single-configuration generators", "CMAKE_BUILD_TYPE", "STRING", "Release")
        self.build_type.possible_values = ["Release", "Debug", "RelWithDebInfo", "MinSizeRel"]
        self.bool_options = {name: cache_value for name, cache_value in cache_values.items() if cache_value.value_type == "BOOL" and "ENABLE" in name}
        self.multi_choice_options = [cache_value for name, cache_value in cache_values.items() if cache_value.value_type == "STRING" and cache_value.possible_values is not None]
        self.build_dir = pathlib.Path(__file__).parent.parent.resolve() / "build"
        self.source_dir = pathlib.Path(__file__).parent.parent.resolve()

    def create_cmake_options_str(self) -> str:
        return ', '.join([bool_option.create_cmake_option_str() for name, bool_option in self.bool_options])

    def is_enabled(self, option_name: str) -> bool:
        if option_name not in self.bool_options:
            print(self.bool_options)
            raise ValueError(f"Expected {option_name} to be a minifi option")
        if "ENABLE_ALL" in self.bool_options and self.bool_options["ENABLE_ALL"] == "ON":
            return True
        return self.bool_options[option_name].value == "ON"


def parse_minifi_options(path: str):
    cmake_cache_path = cmake_parser.create_cmake_cache(path)
    return MinifiOptions(cmake_parser.parse_cmake_cache_values(cmake_cache_path))
