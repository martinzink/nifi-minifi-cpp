import tempfile
import time
from typing import Dict, Any, List

import docker
import logging

import humanfriendly

from containers.file import File


class Container:
    def __init__(self, image_name: str, container_name: str, network: str = None):
        self.image_name: str = image_name
        self.container_name: str = container_name
        self.network: str | None = network

        self.client = docker.from_env()
        self.container = None
        self.files: List[File] = []
        self.volumes = None
        self.command = None
        self._temp_dir = None


    def deploy(self):
        self._temp_dir = tempfile.TemporaryDirectory()

        if len(self.files) != 0:
            self.volumes = {}
            for file in self.files:
                temp_path = self._temp_dir.name + "/" + file.host_filename
                with open(temp_path, "w") as temp_file:
                    temp_file.write(file.content)
                self.volumes[temp_path] = {
                    "bind": file.path,
                    "mode": file.mode
                }

        try:
            existing_container = self.client.containers.get(self.container_name)
            logging.warn(f"Found existing container '{self.container_name}'. Removing it first.")
            existing_container.remove(force=True)
        except docker.errors.NotFound:
            pass # No existing container found, which is good.
        try:
            print(f"Creating and starting container '{self.container_name}'...")
            self.container = self.client.containers.run(
                image=self.image_name,
                name=self.container_name,
                ports=None,
                environment=None,
                volumes=self.volumes,
                network=None,
                command=self.command,
                detach=True  # Always run in the background
            )
        except Exception as e:
            logging.error(f"Error starting container: {e}")
            raise
        return True

    def clean_up(self):
        self.container.remove(force=True)

    def exec_run(self, command):
        """Executes a command inside the running container."""
        if self.container:
            # Note: exec_run returns a tuple (exit_code, output)
            return self.container.exec_run(command)
        return None, "Container not running.".encode('utf-8')

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container:
            return False


        command = f"sh -c \"grep -l -F -- '{expected_content}' {directory_path}/*\""
        exit_code, output = self.exec_run(command)

        if exit_code == 0:
            found_files = output.decode('utf-8').strip().split('\n')
            return True
        elif exit_code == 1:
            return False
        else:
            error_message = output.decode('utf-8').strip()
            return False



    def get_logs(self):
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
