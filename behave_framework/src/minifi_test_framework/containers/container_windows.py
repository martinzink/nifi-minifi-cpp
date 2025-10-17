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
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
import logging
import os
import shlex
import tempfile
import subprocess
from subprocess import check_output

import docker
import logging

from minifi_test_framework.containers.container_protocol import ContainerProtocol
from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.file import File
from docker.models.networks import Network

from minifi_test_framework.containers.host_file import HostFile


class WindowsContainer(ContainerProtocol):
    def __init__(self, image_name: str, container_name: str, network: Network):
        super().__init__()
        self.image_name: str = image_name
        self.container_name: str = container_name
        self.network: Network = network
        self.user: str = "0:0"
        self.client = docker.from_env()
        self.container = None
        self.files: list[File] = []
        self.dirs: list[Directory] = []
        self.host_files: list[HostFile] = []
        self.volumes = {}
        self.command = None
        self._temp_dir = None
        self.ports = None
        self.environment: list[str] = []

    def deploy(self) -> bool:
        self._temp_dir = tempfile.TemporaryDirectory()

        for directory in self.dirs:
            temp_path = self._temp_dir.name + directory.path

            for file_name, content in directory.files.items():
                file_path = temp_path + "\\" + file_name
                os.makedirs(temp_path, exist_ok=True)
                with open(file_path, "w") as temp_file:
                    logging.info(f"writing {content} into {temp_file}")
                    temp_file.write(content)
            self.volumes[temp_path] = {
                "bind": "C:" + directory.path,
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
            self.container = self.client.containers.create(
                image=self.image_name,
                name=self.container_name,
                ports=self.ports,
                environment=self.environment,
                volumes=self.volumes,
                network=self.network.name,
                command=self.command,
                detach=True  # Always run in the background
            )
            for file in self.files:
                temp_path = self._temp_dir.name + "/" + file.host_filename
                with open(temp_path, "w") as temp_file:
                    temp_file.write(file.content)

                check_output(f"docker cp {temp_path} {self.container_name}:C:{file.path}\\{file.host_filename}", shell=True, stderr=subprocess.STDOUT)
            
            self.container.start()
        except Exception as e:
            logging.error(f"Error starting container: {e}")
            raise
        return True

    def clean_up(self):
        if self.container:
            self.container.remove(force=True)

    def exec_run(self, command) -> tuple[int | None, str]:
        logging.info(f"Running {command}")
        if self.container:
            (code, output) = self.container.exec_run(command)
            logging.info(f"Result {code}, and {output.decode("utf-8")}")
            return code, output.decode("utf-8")
        return None, "Container not running."

    def not_empty_dir_exists(self, directory_path: str) -> bool:
        directory_path = directory_path.removeprefix("/")
        if not self.container:
            return False
        
        ps_command = (
            f"if (Test-Path -Path '{directory_path}' -PathType Container) {{ "
            f"  if (Get-ChildItem -Path '{directory_path}') {{ exit 0 }} else {{ exit 1 }} "
            f"}} else {{ exit 2 }}"
        )
        full_command = f"powershell -NonInteractive -NoProfile -Command \"{ps_command}\""
        dir_exists_exit_code, _ = self.exec_run(full_command)
        
        return dir_exists_exit_code == 0

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container or not self.not_empty_dir_exists(directory_path):
            return False

        directory_path = directory_path.removeprefix("/")

        escaped_content = expected_content.replace("'", "''")
        
        command = f"powershell -NonInteractive -NoProfile -Command \"Get-ChildItem -Path '{directory_path}' -File -Depth 0 | Select-String -Pattern '{escaped_content}' -SimpleMatch -List\""

        exit_code, output = self.exec_run(command)

        return exit_code == 0

    def directory_contains_file_with_regex(self, directory_path: str, regex_str: str) -> bool:
        if not self.container or not self.not_empty_dir_exists(directory_path):
            return False

        directory_path = directory_path.removeprefix("/")

        escaped_regex = regex_str.replace("'", "''")
        
        # Get-ChildItem replaces 'find'. Select-String (without -SimpleMatch) replaces 'grep -E'.
        command = f"powershell -NonInteractive -NoProfile -Command \"Get-ChildItem -Path '{directory_path}' -File -Depth 0 | Select-String -Pattern '{escaped_regex}' -List\""
        
        exit_code, output = self.exec_run(command)
        return exit_code == 0
    
    def path_with_content_exists(self, path: str, content: str) -> bool:
        if not self.container:
            return False

        escaped_content = content.replace("'", "''").replace("\n", "\r\n")
        
        # Find exact line matches (^) and ($) and check if the Count is 1.
        # A try-catch block makes the PowerShell command more robust against file-not-found.
        command = (
            f"powershell -NonInteractive -NoProfile -Command \""
            f"try {{ "
            f"  $count = (Select-String -Path '{path}' -Pattern '^{escaped_content}$').Count; "
            f"  if ($count -eq 1) {{ exit 0 }} else {{ exit 1 }} "
            f"}} catch {{ exit 2 }}\""
        )
        
        exit_code, output = self.exec_run(command)
        if exit_code != 0:
            logging.error(f"Error running command '{command}': {output}")
            
        return exit_code == 0

    def directory_has_single_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container:
            return False

        escaped_content = expected_content.strip().replace("'", "''").replace("\n", "\r\n")

        command_parts = [
            f"$files = Get-ChildItem -Path '{directory_path}' -File -Depth 0;",
            "if ($files.Count -ne 1) {{ exit 1 }};",
            "$actual_content = Get-Content -Path $files[0].FullName -Raw;",
            f"if ($actual_content.Trim() -eq '{escaped_content}') {{ exit 0 }} else {{ exit 2 }};"
        ]
        command = f"powershell -NonInteractive -NoProfile -Command \"{ ' '.join(command_parts) }\""

        exit_code, output = self.exec_run(command)

        if exit_code != 0:
            logging.error(f"Check for single file failed (Code {exit_code}): {output}")
            
        return exit_code == 0

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

    def get_number_of_files(self, directory_path: str) -> int:
        if not self.container:
            return -1

        command = f"powershell -NonInteractive -NoProfile -Command \"(Get-ChildItem -Path '{directory_path}' -File -Depth 0).Count\""
        exit_code, output = self.exec_run(command)

        if exit_code != 0:
            logging.error(f"Error running command '{command}': {output}")
            return -1

        try:
            return int(output.strip())
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{command}'")
            return -1

    def verify_file_contents(self, directory_path: str, expected_contents: list[str]) -> bool:
        raise NotImplemented