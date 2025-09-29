import logging
import random
import string

import humanfriendly
from behave import when, step

from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.file import File
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


@when("both instances start up")
@when("all instances start up")
def step_impl(context: MinifiTestContext):
    for container in context.containers:
        assert container.deploy()
    assert context.minifi_container.deploy() or context.minifi_container.get_logs()
    logging.debug("All instances started up")


@when("the MiNiFi instance starts up")
def step_impl(context):
    assert context.minifi_container.deploy()
    logging.debug("All instances started up")


@step('a directory at "{directory}" has a file with the size "{size}"')
def step_impl(context: MinifiTestContext, directory: str, size: str):
    size = humanfriendly.parse_size(size)
    content = ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(size))
    new_dir = Directory(directory)
    new_dir.files["input.txt"] = content
    context.minifi_container.dirs.append(new_dir)


@step('a file with filename "{file_name}" and content "{content}" is present in "{path}"')
def step_impl(context: MinifiTestContext, file_name: str, content: str, path: str):
    new_content = content.replace("\\n", "\n")
    context.minifi_container.files.append(File(path, file_name, new_content))
