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


# MQTT setup
@when("an MQTT broker is set up in correspondence with the PublishMQTT")
@given("an MQTT broker is set up in correspondence with the PublishMQTT")
@given("an MQTT broker is set up in correspondence with the ConsumeMQTT")
@given("an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT")
def step_impl(context):
    context.test.acquire_container(context=context, name="mqtt-broker", engine="mqtt-broker")
    context.test.start('mqtt-broker')

@then("the MQTT broker has a log line matching \"{log_pattern}\"")
def step_impl(context, log_pattern):
    context.test.check_container_log_matches_regex('mqtt-broker', log_pattern, 60, count=1)
# MQTT

@then("the MQTT broker has {log_count} log lines matching \"{log_pattern}\"")
def step_impl(context, log_count, log_pattern):
    context.test.check_container_log_matches_regex('mqtt-broker', log_pattern, 60, count=int(log_count))

