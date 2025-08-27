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


from behave import given, when
from minifi.controllers.CouchbaseClusterService import CouchbaseClusterService

@when(u'a Couchbase server is started')
def step_impl(context):
    context.test.start_couchbase_server(context)


@given("a CouchbaseClusterService is setup up with the name \"{service_name}\"")
def step_impl(context, service_name):
    couchbase_cluster_controller_service = CouchbaseClusterService(
        name=service_name,
        connection_string="couchbase://{server_hostname}".format(server_hostname=context.test.get_container_name_with_postfix("couchbase-server")))
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    container.add_controller(couchbase_cluster_controller_service)


@given("a CouchbaseClusterService is set up up with SSL connection with the name \"{service_name}\"")
def step_impl(context, service_name):
    ssl_context_service = SSLContextService(name="SSLContextService",
                                            ca_cert='/tmp/resources/root_ca.crt')
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    container.add_controller(ssl_context_service)
    couchbase_cluster_controller_service = CouchbaseClusterService(
        name=service_name,
        connection_string="couchbases://{server_hostname}".format(server_hostname=context.test.get_container_name_with_postfix("couchbase-server")),
        ssl_context_service=ssl_context_service)
    container.add_controller(couchbase_cluster_controller_service)


@then("a document with id \"{doc_id}\" in bucket \"{bucket_name}\" is present with data '{data}' of type \"{data_type}\" in Couchbase")
def step_impl(context, doc_id: str, bucket_name: str, data: str, data_type: str):
    context.test.check_is_data_present_on_couchbase(doc_id, bucket_name, data, data_type)


@given("a CouchbaseClusterService is setup up using mTLS authentication with the name \"{service_name}\"")
def step_impl(context, service_name):
    ssl_context_service = SSLContextService(name="SSLContextService",
                                            cert='/tmp/resources/clientuser.crt',
                                            key='/tmp/resources/clientuser.key',
                                            ca_cert='/tmp/resources/root_ca.crt')
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    container.add_controller(ssl_context_service)
    couchbase_cluster_controller_service = CouchbaseClusterService(
        name=service_name,
        connection_string="couchbases://{server_hostname}".format(server_hostname=context.test.get_container_name_with_postfix("couchbase-server")),
        ssl_context_service=ssl_context_service)
    container.add_controller(couchbase_cluster_controller_service)

