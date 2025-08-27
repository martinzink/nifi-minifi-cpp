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

@given("a Prometheus server is set up")
def step_impl(context):
    context.test.acquire_container(context=context, name="prometheus", engine="prometheus")


@given("a Prometheus server is set up with SSL")
def step_impl(context):
    context.test.acquire_container(context=context, name="prometheus", engine="prometheus-ssl")


@then("\"{metric_class}\" are published to the Prometheus server in less than {timeout_seconds:d} seconds")
@then("\"{metric_class}\" is published to the Prometheus server in less than {timeout_seconds:d} seconds")
def step_impl(context, metric_class, timeout_seconds):
    context.test.check_metric_class_on_prometheus(metric_class, timeout_seconds)


@then("\"{metric_class}\" processor metric is published to the Prometheus server in less than {timeout_seconds:d} seconds for \"{processor_name}\" processor")
def step_impl(context, metric_class, timeout_seconds, processor_name):
    context.test.check_processor_metric_on_prometheus(metric_class, timeout_seconds, processor_name)


@then("all Prometheus metric types are only defined once")
def step_impl(context):
    context.test.check_all_prometheus_metric_types_are_defined_once()