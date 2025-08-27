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


from behave import given

@given(u'there is an accessible PLC with modbus enabled')
def step_impl(context):
    context.test.acquire_container(context=context, name="diag-slave-tcp", engine="diag-slave-tcp")
    context.test.start('diag-slave-tcp')


@given(u'PLC register has been set with {modbus_cmd} command')
def step_impl(context, modbus_cmd):
    context.test.set_value_on_plc_with_modbus(context.test.get_container_name_with_postfix('diag-slave-tcp'), modbus_cmd)
