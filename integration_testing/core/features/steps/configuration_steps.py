from behave import *

from core.minifi_test_context import MinifiTestContext


@step('MiNiFi configuration "{config_key}" is set to "{config_value}"')
def step_impl(context: MinifiTestContext, config_key: str, config_value: str):
    context.minifi_container.set_property(config_key, config_value)


@step("log metrics publisher is enabled in MiNiFi")
def step_impl(context):
    context.minifi_container.set_property("nifi.metrics.publisher.LogMetricsPublisher.metrics", "RepositoryMetrics")
    context.minifi_container.set_property("nifi.metrics.publisher.LogMetricsPublisher.logging.interval", "1s")
    context.minifi_container.set_property("nifi.metrics.publisher.class", "LogMetricsPublisher")
