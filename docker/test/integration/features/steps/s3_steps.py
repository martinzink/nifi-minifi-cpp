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


from behave import given, then

@given("a s3 server is set up in correspondence with the PutS3Object")
@given("a s3 server is set up in correspondence with the DeleteS3Object")
def step_impl(context):
    context.test.acquire_container(context=context, name="s3-server", engine="s3-server")

@then("the object on the s3 server is \"{object_data}\"")
def step_impl(context, object_data):
    context.test.check_s3_server_object_data("s3-server", object_data)


@then("the object on the s3 server is present and matches the original hash")
def step_impl(context):
    context.test.check_s3_server_large_object_data("s3-server")


@then("the object content type on the s3 server is \"{content_type}\" and the object metadata matches use metadata")
def step_impl(context, content_type):
    context.test.check_s3_server_object_metadata("s3-server", content_type)


@then("the object bucket on the s3 server is empty")
def step_impl(context):
    context.test.check_empty_s3_bucket("s3-server")
