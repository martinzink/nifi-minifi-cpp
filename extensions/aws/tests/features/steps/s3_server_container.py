import json
import logging

from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class S3ServerContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("adobe/s3mock:3.12.0", f"s3-server-{test_context.scenario_id}", test_context.network)
        self.environment.append("initialBuckets=test_bucket")

    def deploy(self):
        super().deploy()
        finished_str = "Started S3MockApplication"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=15,
            bail_condition=lambda: self.exited,
            context=None)

    def check_kinesis_server_record_data(self, container_name, record_data):
        (code, output) = self.exec_run(["node", "/app/consumer/consumer.js", record_data])
        return code == 0

    def check_s3_server_object_data(self, test_data):
        (code, output) = self.exec_run(["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        if code != 0:
            return False
        s3_mock_dir = output.strip()
        (code, file_data) = self.exec_run(["cat", s3_mock_dir + "/binaryData"])
        return code == 0 and file_data == test_data

    def check_s3_server_object_hash(self, expected_file_hash: str):
        (code, output) = self.exec_run(["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        if code != 0:
            return False
        dir_candidates = output.split("\n")
        for candidate in dir_candidates:
            if "multiparts" not in candidate:
                s3_mock_dir = candidate
                break
        (code, md5_output) = self.exec_run(["md5sum", s3_mock_dir + "/binaryData"])
        if code != 0:
            return False
        file_hash = md5_output.split(' ')[0].strip()
        return file_hash == expected_file_hash

    def check_s3_server_object_metadata(self, content_type="application/octet-stream", metadata=dict()):
        (code, output) = self.exec_run(["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        if code != 0:
            return False
        s3_mock_dir = output.strip()
        (code, output) = self.exec_run(["cat", s3_mock_dir + "/objectMetadata.json"])
        server_metadata = json.loads(output)
        return code == 0 and server_metadata["contentType"] == content_type and metadata == server_metadata["userMetadata"]

    def is_s3_bucket_empty(self):
        (code, output) = self.exec_run(["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        logging.info(f"is_s3_bucket_empty: {output}")
        return code == 0 and not output.strip()
