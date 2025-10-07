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

@ENABLE_LUA_SCRIPT
Feature: MiNiFi can execute Lua scripts
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: ExecuteScript should only allow the number of parallel tasks defined by the max concurrent tasks attribute for Lua scripts
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "500 ms"
    And a ExecuteScript processor with the "Script File" property set to "/tmp/resources/lua/sleep_forever.lua"
    And the "Script Engine" property of the ExecuteScript processor is set to "lua"
    And the max concurrent tasks attribute of the ExecuteScript processor is set to 3
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExecuteScript

    When all instances start up
    Then the Minifi logs contain the following message: "Sleeping forever" 3 times after 5 seconds

  Scenario: Lua can reverse incoming flowfile
    Given a GenerateFlowFile processor with the "Custom Text" property set to "for_lua"
    And the scheduling period of the GenerateFlowFile processor is set to "1 hour"
    And the "Data Format" property of the GenerateFlowFile processor is set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And a ExecuteScript processor with the "Script File" property set to "/tmp/resources/lua/reverse_flow_file_content.lua"
    And the "Script Engine" property of the ExecuteScript processor is set to "lua"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExecuteScript
    And the "success" relationship of the ExecuteScript processor is connected to the PutFile

    When all instances start up
    Then a flowfile with the content "aul_rof" is placed in the monitored directory in less than 45 seconds
