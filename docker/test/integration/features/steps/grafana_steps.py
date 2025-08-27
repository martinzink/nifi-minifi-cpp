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

@given("a Grafana Loki server is set up")
def step_impl(context):
    context.test.acquire_container(context=context, name="grafana-loki-server", engine="grafana-loki-server")


@given("a Grafana Loki server with SSL is set up")
def step_impl(context):
    context.test.enable_ssl_in_grafana_loki()
    context.test.acquire_container(context=context, name="grafana-loki-server", engine="grafana-loki-server")


@given("a Grafana Loki server is set up with multi-tenancy enabled")
def step_impl(context):
    context.test.enable_multi_tenancy_in_grafana_loki()
    context.test.acquire_container(context=context, name="grafana-loki-server", engine="grafana-loki-server")


@then("\"{lines}\" lines are published to the Grafana Loki server in less than {timeout_seconds:d} seconds")
@then("\"{lines}\" line is published to the Grafana Loki server in less than {timeout_seconds:d} seconds")
def step_impl(context, lines: str, timeout_seconds: int):
    context.test.check_lines_on_grafana_loki(lines.split(";"), timeout_seconds, False)


@then("\"{lines}\" lines are published to the \"{tenant_id}\" tenant on the Grafana Loki server in less than {timeout_seconds:d} seconds")
@then("\"{lines}\" line is published to the \"{tenant_id}\" tenant on the Grafana Loki server in less than {timeout_seconds:d} seconds")
def step_impl(context, lines: str, tenant_id: str, timeout_seconds: int):
    context.test.check_lines_on_grafana_loki(lines.split(";"), timeout_seconds, False, tenant_id)


@then("\"{lines}\" lines are published using SSL to the Grafana Loki server in less than {timeout_seconds:d} seconds")
@then("\"{lines}\" line is published using SSL to the Grafana Loki server in less than {timeout_seconds:d} seconds")
def step_impl(context, lines: str, timeout_seconds: int):
    context.test.check_lines_on_grafana_loki(lines.split(";"), timeout_seconds, True)

@given(u'a reverse proxy is set up to forward requests to the Grafana Loki server')
def step_impl(context):
    context.test.acquire_container(context=context, name="reverse-proxy", engine="reverse-proxy")

@given(u'a SSL context service is set up for Grafana Loki processor \"{processor_name}\"')
def step_impl(context, processor_name: str):
    setUpSslContextServiceForProcessor(context, processor_name)