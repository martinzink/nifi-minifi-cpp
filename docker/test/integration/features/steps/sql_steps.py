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
from minifi.controllers.ODBCService import ODBCService

@given("an ODBCService is setup up for {processor_name} with the name \"{service_name}\"")
def step_impl(context, processor_name, service_name):
    odbc_service = ODBCService(name=service_name,
                               connection_string="Driver={{PostgreSQL ANSI}};Server={server_hostname};Port=5432;Database=postgres;Uid=postgres;Pwd=password;".format(server_hostname=context.test.get_container_name_with_postfix("postgresql-server")))
    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(odbc_service)
    processor.set_property("DB Controller Service", odbc_service.name)


@given("a PostgreSQL server is set up")
def step_impl(context):
    context.test.enable_sql_in_minifi()
    context.test.acquire_container(context=context, name="postgresql-server", engine="postgresql-server")


@then("the query \"{query}\" returns {number_of_rows:d} rows in less than {timeout_seconds:d} seconds on the PostgreSQL server")
def step_impl(context, query, number_of_rows, timeout_seconds):
    context.test.check_query_results(context.test.get_container_name_with_postfix("postgresql-server"), query, number_of_rows, timeout_seconds)
