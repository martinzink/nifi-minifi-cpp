import logging

from behave import when, step

from containers.file import File
from core.minifi_test_context import MinifiTestContext


@when("all instances start up")
def step_impl(context: MinifiTestContext):
    for container in context.containers:
        assert container.deploy()
    assert context.minifi_container.deploy()
    logging.debug("All instances started up")


@when("the MiNiFi instance starts up")
def step_impl(context):
    assert context.minifi_container.deploy()
    logging.debug("All instances started up")


@step('a file with filename "{file_name}" and content "{content}" is present in "{path}"')
def step_impl(context: MinifiTestContext, file_name: str, content: str, path: str):
    context.minifi_container.files.append(File(path + "/" + file_name, file_name, content))
