from typing import List

from behave.runner import Context
from docker.models.networks import Network

from minifi_test_framework.containers.container_protocol import ContainerProtocol
from minifi_test_framework.containers.minifi_protocol import MinifiProtocol

class MinifiContainer(ContainerProtocol, MinifiProtocol):
    pass

class MinifiTestContext(Context):
    minifi_container: MinifiContainer
    containers: List[ContainerProtocol]
    scenario_id: str
    network: Network
