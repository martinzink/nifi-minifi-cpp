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


import logging
import shortuuid
from behave.model import Feature, Scenario
from behave.runner import Context

from containers.MinifiContainer import MinifiContainer
from core.minifi_test_context import MinifiTestContext


def before_feature(context: Context, feature: Feature):
    logging.info("Running feature: %s", feature)
    context.feature_id = shortuuid.uuid()

def before_scenario(context: Context, scenario: Scenario):
    logging.info("Running scenario: %s", scenario)
    context.minifi_container = MinifiContainer("apacheminificpp:1.0.0", f"minifi-{context.feature_id}")
    context.containers = []
    for step in scenario.steps:
        __inject_feature_id(context, step)

def after_scenario(context: MinifiTestContext, _scenario: Scenario):
    context.minifi_container.clean_up()
    pass

def after_feature(_context: Context, _feature: Feature):
    pass

def __inject_feature_id(context: Context, step):
    if "${feature_id}" in step.name:
        step.name = step.name.replace("${feature_id}", context.feature_id)
    if step.table:
        for row in step.table:
            for i in range(len(row.cells)):
                if "${feature_id}" in row.cells[i]:
                    row.cells[i] = row.cells[i].replace("${feature_id}", context.feature_id)

def before_all(context: Context):
    pass

