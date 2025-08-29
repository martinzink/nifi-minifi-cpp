import os
import tempfile
from typing import Dict

from .Container import Container

from minifi.flow_definition import FlowDefinition
from .file import File


class MinifiContainer(Container):
    def __init__(self, image_name: str, container_name: str):
        super().__init__(image_name, container_name)
        self.flow_config_str: str = ""
        self.flow_definition = FlowDefinition()
        self.properties: Dict[str, str] = {}

        self._fill_default_properties()


    def deploy(self) -> bool:
        self.files.append(File("/opt/minifi/minifi-current/conf/config.yml", "config.yml", self.flow_definition.to_yaml()))
        self.files.append(File("/opt/minifi/minifi-current/conf/minifi.properties", "minifi.properties", self._get_properties_file_content()))

        return super().deploy()

    def set_property(self, key: str, value: str):
        self.properties[key] = value

    def _fill_default_properties(self):
        self.properties["nifi.flow.configuration.file"] = "./conf/config.yml"
        self.properties["nifi.administrative.yield.duration"] = "30 sec"
        self.properties["nifi.bored.yield.duration"] = "100 millis"
        self.properties["nifi.extension.path"] = "../extensions/*"
        self.properties["nifi.openssl.fips.support.enable"] = "false"
        self.properties["nifi.provenance.repository.class.name"] = "NoOpRepository"

        pass

    def _get_properties_file_content(self):
        lines = (f"{key}={value}" for key, value in self.properties.items())
        return "\n".join(lines)