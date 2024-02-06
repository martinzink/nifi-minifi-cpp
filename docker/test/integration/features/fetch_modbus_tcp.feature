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

@CORE
Feature: Minifi C++ can act as a modbus tcp master

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: MiNiFi can fetch data from a modbus slave
    Given a FetchModbusTcp processor
    And a AttributesToJSON processor with the "Attributes List" property set to "foo,bar,baz"
    And the "Destination" property of the AttributesToJSON processor is set to "flowfile-content"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "Protocol" property of the FetchModbusTcp processor is set to "UDP"
    And there is an accessible PLC with modbus enabled
    And the PLC has 123 in its 52th holding register
    And the PLC has 6.78 in its 5678th holding register
    And the PLC has 'M','i','N','i','F','i' in its input registers starting from 4444
    And the "success" relationship of the FetchModbusTcp processor is connected to the AttributesToJSON
    And the "success" relationship of the AttributesToJSON processor is connected to the PutFile
    And the "foo" property of the FetchModbusTcp processor is set to "holding-register:52"
    And the "bar" property of the FetchModbusTcp processor is set to "405678:REAL"
    And the "baz" property of the FetchModbusTcp processor is set to "4x4444:CHAR[6]"

    When both instances start up
    Then a flowfile with the JSON content "{"foo":"123","bar":"6.78","baz":"M,i,N,i,F,i"}" is placed in the monitored directory in less than 10 seconds
