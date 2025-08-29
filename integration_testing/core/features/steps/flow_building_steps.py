from behave import given, step

from core.minifi_test_context import MinifiTestContext
from minifi.connection import Connection
from minifi.funnel import Funnel
from minifi.parameter import Parameter
from minifi.parameter_context import ParameterContext
from minifi.processor import Processor


@given("a transient MiNiFi flow with a LogOnDestructionProcessor processor")
def step_impl(context: MinifiTestContext):
    context.minifi_container.command = ["/bin/sh", "-c", "timeout 10s ./bin/minifi.sh run && sleep 100"]
    context.minifi_container.flow_definition.add_processor(
        Processor("LogOnDestructionProcessor", "LogOnDestructionProcessor"))


@given(
    'a {processor_type} processor with the name "{processor_name}" and the "{property_name}" property set to "{property_value}"')
def step_impl(context: MinifiTestContext, processor_type: str, processor_name: str, property_name: str,
              property_value: str):
    processor = Processor(processor_type, processor_name)
    processor.add_property(property_name, property_value)
    context.minifi_container.flow_definition.add_processor(processor)


@step('a {processor_type} processor with the "{property_name}" property set to "{property_value}"')
def step_impl(context: MinifiTestContext, processor_type: str, property_name: str, property_value: str):
    context.execute_steps(
        f'Given a {processor_type} processor with the name "{processor_type}" and the "{property_name}" property set to "{property_value}"')


@given('a {processor_type} processor with the name "{processor_name}"')
def step_impl(context: MinifiTestContext, processor_type: str, processor_name: str):
    processor = Processor(processor_type, processor_name)
    context.minifi_container.flow_definition.add_processor(processor)


@given("a {processor_type} processor")
def step_impl(context: MinifiTestContext, processor_type: str):
    processor = Processor(processor_type, processor_type)
    context.minifi_container.flow_definition.add_processor(processor)


@step('the "{property_name}" property of the {processor_name} processor is set to "{property_value}"')
def step_impl(context: MinifiTestContext, property_name: str, processor_name: str, property_value: str):
    processor = context.minifi_container.flow_definition.get_processor(processor_name)
    processor.add_property(property_name, property_value)


@step('a Funnel with the name "{funnel_name}" is set up')
def step_impl(context: MinifiTestContext, funnel_name: str):
    context.minifi_container.flow_definition.add_funnel(Funnel(funnel_name))


@step('the "{relationship_name}" relationship of the {source} processor is connected to the {target}')
def step_impl(context: MinifiTestContext, relationship_name: str, source: str, target: str):
    connection = Connection(source_name=source, source_relationship=relationship_name, target_name=target)
    context.minifi_container.flow_definition.add_connection(connection)


@step('the Funnel with the name "{funnel_name}" is connected to the {target}')
def step_impl(context: MinifiTestContext, funnel_name: str, target: str):
    connection = Connection(source_name=funnel_name, source_relationship="success", target_name=target)
    context.minifi_container.flow_definition.add_connection(connection)


@step("{processor_name}'s success relationship is auto-terminated")
def step_impl(context: MinifiTestContext, processor_name: str):
    context.minifi_container.flow_definition.get_processor(processor_name).auto_terminated_relationships.append(
        "success")


@given("a transient MiNiFi flow is set up")
def step_impl(context: MinifiTestContext):
    context.minifi_container.command = ["/bin/sh", "-c", "timeout 10s ./bin/minifi.sh run && sleep 100"]


@step('the scheduling period of the {processor_name} processor is set to "{duration_str}"')
def step_impl(context: MinifiTestContext, processor_name: str, duration_str: str):
    context.minifi_container.flow_definition.get_processor(processor_name).scheduling_period = duration_str


@given("parameter context name is set to '{context_name}'")
def step_impl(context: MinifiTestContext, context_name: str):
    context.minifi_container.flow_definition.parameter_contexts.append(ParameterContext(context_name))


@step("a non-sensitive parameter in the flow config called '{parameter_name}' with the value '{parameter_value}' in the parameter context '{context_name}'")
def step_impl(context: MinifiTestContext, parameter_name: str, parameter_value: str, context_name: str):
    parameter_context = context.minifi_container.flow_definition.get_parameter_context(context_name)
    parameter_context.parameters.append(Parameter(parameter_name, parameter_value, False))

