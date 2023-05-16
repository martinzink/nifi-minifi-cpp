from minifi_option import MinifiOptions
from system_dependency import SystemDependency
import inquirer
import os


def install_dependencies(minifi_options: MinifiOptions):
    SystemDependency.install_required(minifi_options)


def run_cmake(minifi_options: MinifiOptions):
    if not os.path.exists(minifi_options.build_dir):
        os.mkdir(minifi_options.build_dir)
    os.chdir(minifi_options.build_dir)
    os.system(f"cmake {minifi_options.source_dir}")


def do_build(minifi_options: MinifiOptions):
    os.chdir(minifi_options.build_dir)
    os.system(f"cmake --build .")


def do_one_click_build(minifi_options: MinifiOptions) -> bool:
    install_dependencies(minifi_options)
    run_cmake(minifi_options)
    do_build(minifi_options)
    return True


def modify_bool_options(minifi_options: MinifiOptions):
    options = [inquirer.Checkbox(
        "MiNiFi C++ options",
        message="Select MiNiFi C++ components",
        choices=[name for name, obj in minifi_options.bool_options.items()],
        default=[name for name, obj in minifi_options.bool_options.items() if obj.value == "ON"]
    )]

    selection_result = inquirer.prompt(options)
    for minifi_option in minifi_options.bool_options.values():
        if minifi_option.name in selection_result:
            minifi_option.value = "ON"
        else:
            minifi_option.value = "OFF"


def main_menu(minifi_options: MinifiOptions):
    done = False
    while not done:
        main_menu_options = {
            f"Build dir: {minifi_options.build_dir}": build_dir_menu,
            f"Build type: {minifi_options.build_type.value}": build_type_menu,
            f"Build options": bool_menu,
            "One click build": do_one_click_build,
            "Step by step build": step_by_step_menu,
            "Exit": lambda options: True,
        }

        questions = [
            inquirer.List(
                "sub_menu",
                message="Main Menu",
                choices=[menu_option_name for menu_option_name in main_menu_options],
            ),
        ]

        main_menu_prompt = inquirer.prompt(questions)
        done = main_menu_options[main_menu_prompt["sub_menu"]](minifi_options)


def build_type_menu(minifi_options: MinifiOptions) -> bool:
    questions = [
        inquirer.List(
            "build_type",
            message="Build type",
            choices=minifi_options.build_type.possible_values,
        ),
    ]

    answers = inquirer.prompt(questions)
    minifi_options.build_type.value = answers["build_type"]
    return False


def build_dir_menu(minifi_options: MinifiOptions) -> bool:
    questions = [
        inquirer.Path('build_dir',
                      message="Build directory",
                      default=minifi_options.build_dir
                      ),
    ]
    minifi_options.build_dir = inquirer.prompt(questions)["build_dir"]
    return False


def bool_menu(minifi_options: MinifiOptions) -> bool:
    possible_values = [option_name for option_name in minifi_options.bool_options]
    selected_values = [option.name for option in minifi_options.bool_options.values() if option.value == "ON"]
    questions = [
        inquirer.Checkbox(
            "options",
            message="MiNiFi C++ Options",
            choices=possible_values,
            default=selected_values
        ),
    ]

    answers = inquirer.prompt(questions)
    for bool_option in minifi_options.bool_options.values():
        if bool_option.name in answers["options"]:
            bool_option.value = "ON"
        else:
            bool_option.value = "OFF"

    return False


def step_by_step_menu(minifi_options: MinifiOptions) -> bool:
    done = False
    while not done:
        step_by_step_options = {
            f"Build dir: {minifi_options.build_dir}": build_dir_menu,
            "Install dependencies": install_dependencies,
            "Run cmake": run_cmake,
            "Build": do_build,
            "Exit": lambda options: True,
        }
        questions = [
            inquirer.List(
                "selection",
                message="Step by step menu",
                choices=[step_by_step_menu_option_name for step_by_step_menu_option_name in step_by_step_options],
            ),
        ]

        step_by_step_prompt = inquirer.prompt(questions)
        done = step_by_step_options[step_by_step_prompt["selection"]](minifi_options)
    return True
