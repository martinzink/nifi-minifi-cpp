#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

import os
import docker

from behave.runner import Context
from docker.models.networks import Network

from minifi_test_framework.containers.container_protocol import ContainerProtocol
from minifi_test_framework.containers.minifi_protocol import MinifiProtocol
from minifi_test_framework.containers.minifi_fhs_container import MinifiFhsContainer
from minifi_test_framework.containers.minifi_linux_container import MinifiLinuxContainer
from minifi_test_framework.containers.minifi_win_container import MinifiWindowsContainer

DEFAULT_MINIFI_CONTAINER_NAME = "minifi-primary"

class MinifiContainer(ContainerProtocol, MinifiProtocol):
    pass

class MinifiTestContext(Context):
    containers: dict[str, ContainerProtocol]
    scenario_id: str
    network: Network
    minifi_container_image: str
    resource_dir: str | None

    def get_or_create_minifi_container(self, container_name: str) -> MinifiContainer:
        if container_name not in self.containers:
            docker_client = docker.client.from_env()
            if os.name == 'nt':
                self.containers[container_name] = MinifiWindowsContainer(self.minifi_container_image, container_name, self.scenario_id, self.network)
            elif 'MINIFI_INSTALLATION_TYPE=FHS' in str(docker_client.images.get(self.minifi_container_image).history()):
                self.containers[container_name] = MinifiFhsContainer(self.minifi_container_image, container_name, self.scenario_id, self.network)
            else:
                self.containers[container_name] = MinifiLinuxContainer(self.minifi_container_image, container_name, self.scenario_id, self.network)
        return self.containers[container_name]

    def get_or_create_default_minifi_container(self) -> MinifiContainer:
        return self.get_or_create_minifi_container(DEFAULT_MINIFI_CONTAINER_NAME)

    def get_minifi_container(self, container_name: str) -> MinifiContainer:
        if container_name not in self.containers:
            raise KeyError(f"MiNiFi container '{container_name}' does not exist in the test context.")
        return self.containers[container_name]

    def get_default_minifi_container(self) -> MinifiContainer:
        return self.get_minifi_container(DEFAULT_MINIFI_CONTAINER_NAME)
