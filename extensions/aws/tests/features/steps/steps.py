import hashlib
import random
import string

import humanfriendly
from behave import step

from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.minifi.processor import Processor
from minifi_test_framework.core.helpers import wait_for_condition

from s3_server_container import S3ServerContainer


@step('a {processor_name} processor set up to communicate with an s3 server')
@step('a {processor_name} processor set up to communicate with the same s3 server')
def step_impl(context: MinifiTestContext, processor_name: str):
    processor = Processor(processor_name, processor_name)
    processor.add_property('Object Key', 'test_object_key')
    processor.add_property('Bucket', 'test_bucket')
    processor.add_property('Access Key', 'test_access_key')
    processor.add_property('Secret Key', 'test_secret')
    processor.add_property('Endpoint Override URL', f"http://s3-server-{context.scenario_id}:9090")
    processor.add_property('Proxy Host', '')
    processor.add_property('Proxy Port', '')
    processor.add_property('Proxy Username', '')
    processor.add_property('Proxy Password', '')

    context.minifi_container.flow_definition.add_processor(processor)


@step('a s3 server is set up in correspondence with the {processor_name}')
@step('an s3 server is set up in correspondence with the {processor_name}')
def step_impl(context: MinifiTestContext, processor_name: str):
    context.containers.append(S3ServerContainer(context))


@step('the object on the s3 server is "{object_data}"')
def step_impl(context: MinifiTestContext, object_data: str):
    s3_server_container = context.containers[0]
    assert isinstance(s3_server_container, S3ServerContainer)
    assert s3_server_container.check_s3_server_object_data(object_data)


@step('the object content type on the s3 server is "{content_type}" and the object metadata matches use metadata')
def step_impl(context: MinifiTestContext, content_type: str):
    s3_server_container = context.containers[0]
    assert isinstance(s3_server_container, S3ServerContainer)
    assert s3_server_container.check_s3_server_object_metadata(content_type)


@step("the object bucket on the s3 server is empty in less than 10 seconds")
def step_impl(context):
    s3_server_container = context.containers[0]
    assert isinstance(s3_server_container, S3ServerContainer)
    assert wait_for_condition(
        condition=lambda: s3_server_container.is_s3_bucket_empty(),
        timeout_seconds=10, bail_condition=lambda: s3_server_container.exited, context=context)


@step("the object on the s3 server is present and matches the original hash")
def step_impl(context):
    s3_server_container = context.containers[0]

    assert isinstance(s3_server_container, S3ServerContainer)
    assert s3_server_container.check_s3_server_object_hash(context.original_hash)


def computeMD5hash(my_string):
    m = hashlib.md5()
    m.update(my_string.encode('utf-8'))
    return m.hexdigest()


@step('there is a 6MB file at the "/tmp/input" directory and we keep track of the hash of that')
def step_impl(context):
    size = humanfriendly.parse_size("6MB")
    content = ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(size))
    new_dir = Directory("/tmp/input")
    new_dir.files["input.txt"] = content
    context.minifi_container.dirs.append(new_dir)
    context.original_hash = computeMD5hash(content)
