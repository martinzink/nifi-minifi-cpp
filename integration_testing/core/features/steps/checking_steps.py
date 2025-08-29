import time

import humanfriendly
from behave import then

from core.helpers import wait_for_condition
from core.minifi_test_context import MinifiTestContext


@then('at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: context.minifi_container.directory_contains_file_with_content(directory, content),
                              timeout_seconds=timeout_in_seconds,
                              bail_condition=lambda: context.minifi_container.exited)


@then('the Minifi logs do not contain the following message: "{message}" after {duration}')
def step_impl(context: MinifiTestContext, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    time.sleep(duration_seconds)
    assert message not in context.minifi_container.get_logs()


@then("the Minifi logs contain the following message: '{message}' in less than {duration}")
@then('the Minifi logs contain the following message: "{message}" in less than {duration}')
def step_impl(context: MinifiTestContext, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: message in context.minifi_container.get_logs(),
                              timeout_seconds=duration_seconds,
                              bail_condition=lambda: context.minifi_container.exited)

