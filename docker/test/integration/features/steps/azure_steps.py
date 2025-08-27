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
@given("an Azure storage server is set up")
def step_impl(context):
    context.test.acquire_container(context=context, name="azure-storage-server", engine="azure-storage-server")

@when("test blob \"{blob_name}\" with the content \"{content}\" is created on Azure blob storage")
def step_impl(context, blob_name, content):
    context.test.add_test_blob(blob_name, content, False)


@when("test blob \"{blob_name}\" with the content \"{content}\" and a snapshot is created on Azure blob storage")
def step_impl(context, blob_name, content):
    context.test.add_test_blob(blob_name, content, True)


@when("test blob \"{blob_name}\" is created on Azure blob storage")
def step_impl(context, blob_name):
    context.test.add_test_blob(blob_name, "", False)


@when("test blob \"{blob_name}\" is created on Azure blob storage with a snapshot")
def step_impl(context, blob_name):
    context.test.add_test_blob(blob_name, "", True)


@then("the object on the Azure storage server is \"{object_data}\"")
def step_impl(context, object_data):
    context.test.check_azure_storage_server_data("azure-storage-server", object_data)


@then("the Azure blob storage becomes empty in {timeout_seconds:d} seconds")
def step_impl(context, timeout_seconds):
    context.test.check_azure_blob_storage_is_empty(timeout_seconds)


@then("the blob and snapshot count becomes {blob_and_snapshot_count:d} in {timeout_seconds:d} seconds")
def step_impl(context, blob_and_snapshot_count, timeout_seconds):
    context.test.check_azure_blob_and_snapshot_count(blob_and_snapshot_count, timeout_seconds)

