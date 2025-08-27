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

from common import *
from minifi.controllers.SSLContextService import SSLContextService
from minifi.controllers.ElasticsearchCredentialsService import ElasticsearchCredentialsService

@given('an Elasticsearch server is set up and running')
@given('an Elasticsearch server is set up and a single document is present with "preloaded_id" in "my_index"')
@given('an Elasticsearch server is set up and a single document is present with "preloaded_id" in "my_index" with "value1" in "field1"')
def step_impl(context):
    context.test.start_elasticsearch(context)
    context.test.create_doc_elasticsearch(context.test.get_container_name_with_postfix("elasticsearch"), "my_index", "preloaded_id")


# opensearch
@given('an Opensearch server is set up and running')
@given('an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index"')
@given('an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index" with "value1" in "field1"')
def step_impl(context):
    context.test.start_opensearch(context)
    context.test.add_elastic_user_to_opensearch(context.test.get_container_name_with_postfix("opensearch"))
    context.test.create_doc_elasticsearch(context.test.get_container_name_with_postfix("opensearch"), "my_index", "preloaded_id")

@given(u'a SSL context service is set up for PostElasticsearch and Elasticsearch')
def step_impl(context):
    setUpSslContextServiceForProcessor(context, "PostElasticsearch")


@given(u'a SSL context service is set up for PostElasticsearch and Opensearch')
def step_impl(context):
    root_ca_crt_file = '/tmp/resources/root_ca.crt'
    ssl_context_service = SSLContextService(ca_cert=root_ca_crt_file)
    post_elasticsearch_json = context.test.get_node_by_name("PostElasticsearch")
    post_elasticsearch_json.controller_services.append(ssl_context_service)
    post_elasticsearch_json.set_property("SSL Context Service", ssl_context_service.name)


@given(u'an ElasticsearchCredentialsService is set up for PostElasticsearch with Basic Authentication')
def step_impl(context):
    elasticsearch_credential_service = ElasticsearchCredentialsService()
    post_elasticsearch_json = context.test.get_node_by_name("PostElasticsearch")
    post_elasticsearch_json.controller_services.append(elasticsearch_credential_service)
    post_elasticsearch_json.set_property("Elasticsearch Credentials Provider Service", elasticsearch_credential_service.name)


@given(u'an ElasticsearchCredentialsService is set up for PostElasticsearch with ApiKey')
def step_impl(context):
    api_key = context.test.elastic_generate_apikey("elasticsearch")
    elasticsearch_credential_service = ElasticsearchCredentialsService(api_key)
    post_elasticsearch_json = context.test.get_node_by_name("PostElasticsearch")
    post_elasticsearch_json.controller_services.append(elasticsearch_credential_service)
    post_elasticsearch_json.set_property("Elasticsearch Credentials Provider Service", elasticsearch_credential_service.name)

@then("Elasticsearch is empty")
def step_impl(context):
    context.test.check_empty_elastic(context.test.get_container_name_with_postfix("elasticsearch"))


@then(u'Elasticsearch has a document with "{doc_id}" in "{index}" that has "{value}" set in "{field}"')
def step_impl(context, doc_id, index, value, field):
    context.test.check_elastic_field_value(context.test.get_container_name_with_postfix("elasticsearch"), index_name=index, doc_id=doc_id, field_name=field, field_value=value)


@then("Opensearch is empty")
def step_impl(context):
    context.test.check_empty_elastic(f"opensearch-{context.feature_id}")


@then(u'Opensearch has a document with "{doc_id}" in "{index}" that has "{value}" set in "{field}"')
def step_impl(context, doc_id, index, value, field):
    context.test.check_elastic_field_value(f"opensearch-{context.feature_id}", index_name=index, doc_id=doc_id, field_name=field, field_value=value)
