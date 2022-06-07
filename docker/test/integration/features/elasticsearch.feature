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

Feature: Sending data to Splunk HEC using PutSplunkHTTP

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to a Elasticsearch with SSL enabled
    Given an Elasticsearch server is set up and running
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "foobar" is present in "/tmp/input"
    And a PutElasticsearchJson processor
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutElasticsearchJson
    And the "success" relationship of the PutElasticsearchJson processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "foobar" is placed in the monitored directory in less than 2000 seconds
