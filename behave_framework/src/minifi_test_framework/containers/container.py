import os
import shlex
import tempfile
from typing import Dict, Any, List

import docker
import logging

from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.file import File
from docker.models.networks import Network

from minifi_test_framework.containers.host_file import HostFile


class Container:
    def __init__(self, image_name: str, container_name: str, network: Network):
        self.image_name: str = image_name
        self.container_name: str = container_name
        self.network: Network = network
        self.user: str = "0:0"
        self.client = docker.from_env()
        self.container = None
        self.files: List[File] = []
        self.dirs: List[Directory] = []
        self.host_files: List[HostFile] = []
        self.volumes = {}
        self.command = None
        self._temp_dir = None
        self.ports = None
        self.environment: List[str] = []

    def deploy(self) -> bool:
        self._temp_dir = tempfile.TemporaryDirectory()

        if len(self.files) != 0 or len(self.dirs) != 0 or len(self.host_files) != 0:
            for file in self.files:
                temp_path = self._temp_dir.name + "/" + file.host_filename
                with open(temp_path, "w") as temp_file:
                    temp_file.write(file.content)
                self.volumes[temp_path] = {
                    "bind": file.path + "/" + file.host_filename,
                    "mode": file.mode
                }
            for directory in self.dirs:
                temp_path = self._temp_dir.name + directory.path
                for file_name, content in directory.files.items():
                    file_path = temp_path + "/" + file_name
                    os.makedirs(temp_path, exist_ok=True)
                    with open(file_path, "w") as temp_file:
                        temp_file.write(content)
                self.volumes[temp_path] = {
                    "bind": directory.path,
                    "mode": directory.mode
                }
            for host_file in self.host_files:
                self.volumes[host_file.container_path] = {
                    "bind": host_file.host_path,
                    "mode": host_file.mode
                }

        try:
            existing_container = self.client.containers.get(self.container_name)
            logging.warning(f"Found existing container '{self.container_name}'. Removing it first.")
            existing_container.remove(force=True)
        except docker.errors.NotFound:
            pass
        try:
            print(f"Creating and starting container '{self.container_name}'...")
            self.container = self.client.containers.run(
                image=self.image_name,
                name=self.container_name,
                ports=self.ports,
                environment=self.environment,
                volumes=self.volumes,
                network=self.network.name,
                command=self.command,
                user=self.user,
                detach=True  # Always run in the background
            )
        except Exception as e:
            logging.error(f"Error starting container: {e}")
            raise
        return True

    def clean_up(self):
        if self.container is not None:
            self.container.remove(force=True)

    def exec_run(self, command):
        if self.container:
            (code, output) = self.container.exec_run(command)
            return code, output.decode("utf-8")
        return None, "Container not running."

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container:
            return False

        escaped_content = expected_content.replace("\"", "\\\"")

        command = f"sh -c \"grep -l -F -- '{escaped_content}' {directory_path}/*\""

        exit_code, output = self.exec_run(command)

        return exit_code == 0

    def directory_contains_file_with_regex(self, directory_path: str, regex_str: str) -> bool:
        if not self.container:
            return False

        safe_dir_path = shlex.quote(directory_path)
        safe_regex_str = shlex.quote(regex_str)

        command = (
            f"find {safe_dir_path} -maxdepth 1 -type f -print0 | "
            f"xargs -0 -r grep -l -E -- {safe_regex_str}"
        )

        exit_code, output = self.exec_run(f"sh -c \"{command}\"")

        return exit_code == 0

    def path_with_content_exists(self, path: str, content: str) -> bool:
        count_command = f"sh -c 'cat {path} | grep \"^{content}$\" | wc -l'"
        exit_code, output = self.exec_run(count_command)
        if exit_code != 0:
            logging.error(f"Error running command '{count_command}': {output}")
            return False

        try:
            file_count = int(output.strip())
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{count_command}'")
            return False
        return file_count == 1

    def directory_has_single_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container:
            return False

        count_command = f"sh -c 'find {directory_path} -maxdepth 1 -type f | wc -l'"
        exit_code, output = self.exec_run(count_command)

        if exit_code != 0:
            logging.error(f"Error running command '{count_command}': {output}")
            return False

        try:
            file_count = int(output.strip())
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{count_command}'")
            return False

        if file_count != 1:
            logging.error(f"{directory_path} has too many or too few ({file_count}) files")
            return False

        content_command = f"sh -c 'cat {directory_path}/*'"
        exit_code, output = self.exec_run(content_command)

        if exit_code != 0:
            logging.error(f"Error running command '{content_command}': {output}")
            return False

        actual_content = output.strip()
        logging.debug(f"Comparing: '{actual_content}' vs {expected_content}")
        return actual_content == expected_content.strip()

    def get_logs(self) -> str:
        logging.debug("Getting logs from container '%s'", self.container_name)
        if not self.container:
            return ""
        logs_as_bytes = self.container.logs()
        return logs_as_bytes.decode('utf-8')

    @property
    def exited(self) -> bool:
        if not self.container:
            return False
        try:
            self.container.reload()
            return self.container.status == 'exited'
        except docker.errors.NotFound:
            self.container = None
            return False
        except Exception:
            return False

    def _get_stats(self) -> Dict[str, Any]:
        self.container.reload()
        return self.container.stats(stream=False)

    def get_number_of_files(self, directory_path: str) -> int:
        if not self.container:
            return False

        count_command = f"sh -c 'find {directory_path} -maxdepth 1 -type f | wc -l'"
        exit_code, output = self.exec_run(count_command)

        if exit_code != 0:
            logging.error(f"Error running command '{count_command}': {output}")
            return False

        try:
            return int(output.strip())
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{count_command}'")
            return -1

    def verify_file_contents(self, directory_path: str, expected_contents: list[str]) -> bool:
        if not self.container:
            return False

        safe_dir_path = shlex.quote(directory_path)
        list_files_command = f"find {safe_dir_path} -mindepth 1 -maxdepth 1 -type f -print0"

        exit_code, output = self.exec_run(f"sh -c \"{list_files_command}\"")

        if exit_code != 0:
            logging.error(f"Error running command '{list_files_command}': {output}")
            return False

        actual_filepaths = [path for path in output.split('\0') if path]

        if len(actual_filepaths) != len(expected_contents):
            logging.debug(f"Expected {len(expected_contents)} files, but found {len(actual_filepaths)}")
            return False

        actual_file_contents = []
        for path in actual_filepaths:
            safe_path = shlex.quote(path)
            read_command = f"cat {safe_path}"

            exit_code, content = self.exec_run(read_command)

            if exit_code != 0:
                error_message = f"Command to read file '{path}' failed with exit code {exit_code}"
                logging.error(error_message)
                return False

            actual_file_contents.append(content)

        return sorted(actual_file_contents) == sorted(expected_contents)

    def log_app_output(self) -> bool:
        logs = self.get_logs()
        logging.info("Logs of container '%s':", self.container_name)
        for line in logs.splitlines():
            logging.info(line)
        return False
