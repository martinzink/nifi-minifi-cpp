from typing import List

from behave.runner import Context

from containers.Container import Container
from containers.MinifiContainer import MinifiContainer

class MinifiTestContext(Context):
    minifi_container: MinifiContainer
    containers: List[Container] = []