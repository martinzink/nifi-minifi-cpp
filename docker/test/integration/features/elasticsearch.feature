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
    And a file with the content "{ "field1" : "value1" }" is present in "/tmp/input"
    And a PutElasticsearchJson processor
    And the "Index" property of the PutElasticsearchJson processor is set to "my_index"
    And the "Id" property of the PutElasticsearchJson processor is set to "my_id"
    And the "Index operation" property of the PutElasticsearchJson processor is set to "index"
    And a SSL context service is set up for PutElasticsearchJson
    And an ElasticsearchCredentialsService is set up for PutElasticsearchJson
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutElasticsearchJson
    And the "success" relationship of the PutElasticsearchJson processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "{ "field1" : "value1" }" is placed in the monitored directory in less than 20 seconds
    And Elasticsearch indexed a document with "my_id"

  Scenario: A MiNiFi instance deletes a document from Elasticsearch with SSL enabled
    Given an Elasticsearch server is set up and a single document is present with "preloaded_id" in "my_index"
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "hello world" is present in "/tmp/input"
    And a PutElasticsearchJson processor
    And the "Index" property of the PutElasticsearchJson processor is set to "my_index"
    And the "Id" property of the PutElasticsearchJson processor is set to "preloaded_id"
    And the "Index operation" property of the PutElasticsearchJson processor is set to "delete"
    And a SSL context service is set up for PutElasticsearchJson
    And an ElasticsearchCredentialsService is set up for PutElasticsearchJson
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutElasticsearchJson
    And the "success" relationship of the PutElasticsearchJson processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "hello world" is placed in the monitored directory in less than 20 seconds
    And Elasticsearch is empty
