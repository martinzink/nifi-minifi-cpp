import argparse
import pathlib

from cli import main_menu, do_one_click_build
from minifi_option import parse_minifi_options
from system_dependency import install_compiler

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--noconfirm', action=argparse.BooleanOptionalAction, default=False,
                        help="Bypass any and all “Are you sure?” messages.")
    parser.add_argument('--override', default="", help="Override the default minifi options")
    parser.add_argument('--noninteractive', action=argparse.BooleanOptionalAction, default=False,
                        help="Initiates the one click build")
    args = parser.parse_args()
    no_confirm = args.noconfirm or args.noninteractive

    compiler_override = install_compiler(no_confirm)
    cmake_overrides = args.override + compiler_override
    path = pathlib.Path(__file__).parent.resolve() / '..' / "cmake" / "MiNiFiOptions.cmake"

    minifi_options = parse_minifi_options(str(path.as_posix()), cmake_overrides)
    minifi_options.no_confirm = no_confirm
    if compiler_override is not None:
        minifi_options.set_compiler_override(compiler_override)

    if args.noninteractive:
        do_one_click_build(minifi_options)
    else:
        main_menu(minifi_options)
