from typing import Dict

from docker.models.networks import Network

from .container import Container

from minifi_test_framework.minifi.flow_definition import FlowDefinition
from minifi_test_framework.containers.file import File


class MinifiContainer(Container):
    def __init__(self, image_name: str, scenario_id: str, network: Network):
        super().__init__(image_name, f"minifi-{scenario_id}", network)
        self.flow_config_str: str = ""
        self.flow_definition = FlowDefinition()
        self.properties: Dict[str, str] = {}
        self.log_properties: Dict[str, str] = {}

        self.is_fhs = 'MINIFI_INSTALLATION_TYPE=FHS' in str(self.client.images.get(image_name).history())

        self._fill_default_properties()
        self._fill_default_log_properties()

    def deploy(self) -> bool:
        if self.is_fhs:
            self.files.append(File("/etc/nifi-minifi-cpp", "config.yml", self.flow_definition.to_yaml()))
            self.files.append(File("/etc/nifi-minifi-cpp", "minifi.properties", self._get_properties_file_content()))
            self.files.append(File("/etc/nifi-minifi-cpp", "minifi-log.properties", self._get_log_properties_file_content()))
        else:
            self.files.append(File("/opt/minifi/minifi-current/conf", "config.yml", self.flow_definition.to_yaml()))
            self.files.append(File("/opt/minifi/minifi-current/conf", "minifi.properties", self._get_properties_file_content()))
            self.files.append(File("/opt/minifi/minifi-current/conf", "minifi-log.properties", self._get_log_properties_file_content()))

        return super().deploy()

    def set_property(self, key: str, value: str):
        self.properties[key] = value

    def set_log_property(self, key: str, value: str):
        self.log_properties[key] = value

    def _fill_default_properties(self):
        if self.is_fhs:
            self.properties["nifi.flow.configuration.file"] = "/etc/nifi-minifi-cpp/config.yml"
            self.properties["nifi.extension.path"] = "/usr/lib64/nifi-minifi-cpp/extensions/*"
        else:
            self.properties["nifi.flow.configuration.file"] = "./conf/config.yml"
            self.properties["nifi.extension.path"] = "../extensions/*"
        self.properties["nifi.administrative.yield.duration"] = "1 sec"
        self.properties["nifi.bored.yield.duration"] = "100 millis"
        self.properties["nifi.openssl.fips.support.enable"] = "false"
        self.properties["nifi.provenance.repository.class.name"] = "NoOpRepository"

    def _fill_default_log_properties(self):
        self.log_properties["spdlog.pattern"] = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"

        self.log_properties["appender.stderr"] = "stderr"
        self.log_properties["logger.root"] = "DEBUG, stderr"
        self.log_properties["logger.org::apache::nifi::minifi"] = "DEBUG, stderr"

    def _get_properties_file_content(self):
        lines = (f"{key}={value}" for key, value in self.properties.items())
        return "\n".join(lines)

    def _get_log_properties_file_content(self):
        lines = (f"{key}={value}" for key, value in self.log_properties.items())
        return "\n".join(lines)
