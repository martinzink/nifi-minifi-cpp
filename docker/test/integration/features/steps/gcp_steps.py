# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


from behave import given, when, then
from minifi.controllers.GCPCredentialsControllerService import GCPCredentialsControllerService

@given("a Google Cloud storage server is set up")
@given("a Google Cloud storage server is set up with some test data")
@given('a Google Cloud storage server is set up and a single object with contents "preloaded data" is present')
def step_impl(context):
    context.test.acquire_container(context=context, name="fake-gcs-server", engine="fake-gcs-server")

@given(u'the {processor_one} processor is set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server')
def step_impl(context, processor_one):
    gcp_controller_service = GCPCredentialsControllerService(credentials_location="Use Anonymous credentials")
    p1 = context.test.get_node_by_name(processor_one)
    p1.controller_services.append(gcp_controller_service)
    p1.set_property("GCP Credentials Provider Service", gcp_controller_service.name)
    processor = context.test.get_node_by_name(processor_one)
    processor.set_property("Endpoint Override URL", f"fake-gcs-server-{context.feature_id}:4443")


@given(u'the {processor_one} and the {processor_two} processors are set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server')
def step_impl(context, processor_one, processor_two):
    gcp_controller_service = GCPCredentialsControllerService(credentials_location="Use Anonymous credentials")
    p1 = context.test.get_node_by_name(processor_one)
    p2 = context.test.get_node_by_name(processor_two)
    p1.controller_services.append(gcp_controller_service)
    p1.set_property("GCP Credentials Provider Service", gcp_controller_service.name)
    p2.controller_services.append(gcp_controller_service)
    p2.set_property("GCP Credentials Provider Service", gcp_controller_service.name)
    processor_one = context.test.get_node_by_name(processor_one)
    processor_one.set_property("Endpoint Override URL", f"fake-gcs-server-{context.feature_id}:4443")
    processor_two = context.test.get_node_by_name(processor_two)
    processor_two.set_property("Endpoint Override URL", f"fake-gcs-server-{context.feature_id}:4443")

# Google Cloud Storage
@then('an object with the content \"{content}\" is present in the Google Cloud storage')
def step_imp(context, content):
    context.test.check_google_cloud_storage("fake-gcs-server", content)


@then("the test bucket of Google Cloud Storage is empty")
def step_impl(context):
    context.test.check_empty_gcs_bucket("fake-gcs-server")
