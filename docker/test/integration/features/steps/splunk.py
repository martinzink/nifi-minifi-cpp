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

from minifi.controllers.SSLContextService import SSLContextService


# splunk hec
@given("a Splunk HEC is set up and running")
def step_impl(context):
    context.test.start_splunk(context)

@then('an event is registered in Splunk HEC with the content \"{content}\"')
def step_imp(context, content):
    context.test.check_splunk_event("splunk", content)


@then('an event is registered in Splunk HEC with the content \"{content}\" with \"{source}\" set as source and \"{source_type}\" set as sourcetype and \"{host}\" set as host')
def step_imp(context, content, source, source_type, host):
    attr = {"source": source, "sourcetype": source_type, "host": host}
    context.test.check_splunk_event_with_attributes("splunk", content, attr)


@given("SSL is enabled for the Splunk HEC and the SSL context service is set up for PutSplunkHTTP and QuerySplunkIndexingStatus")
def step_impl(context):
    minifi_crt_file = '/tmp/resources/minifi_client.crt'
    minifi_key_file = '/tmp/resources/minifi_client.key'
    root_ca_crt_file = '/tmp/resources/root_ca.crt'
    ssl_context_service = SSLContextService(name='SSLContextService', cert=minifi_crt_file, ca_cert=root_ca_crt_file, key=minifi_key_file)

    splunk_cert, splunk_key = make_server_cert(context.test.get_container_name_with_postfix("splunk"), context.root_ca_cert, context.root_ca_key)
    put_splunk_http = context.test.get_node_by_name("PutSplunkHTTP")
    put_splunk_http.controller_services.append(ssl_context_service)
    put_splunk_http.set_property("SSL Context Service", ssl_context_service.name)
    query_splunk_indexing_status = context.test.get_node_by_name("QuerySplunkIndexingStatus")
    query_splunk_indexing_status.controller_services.append(ssl_context_service)
    query_splunk_indexing_status.set_property("SSL Context Service", ssl_context_service.name)
    context.test.enable_splunk_hec_ssl('splunk', OpenSSL.crypto.dump_certificate(OpenSSL.crypto.FILETYPE_PEM, splunk_cert), OpenSSL.crypto.dump_privatekey(OpenSSL.crypto.FILETYPE_PEM, splunk_key), OpenSSL.crypto.dump_certificate(OpenSSL.crypto.FILETYPE_PEM, context.root_ca_cert))
