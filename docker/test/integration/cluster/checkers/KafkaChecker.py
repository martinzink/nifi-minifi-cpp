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
from __future__ import annotations

import json
from utils import retry_check
import logging


class KafkaChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    def create_topic(self, container_name: str, topic_name: str):
        logging.info(f"Creating topic {topic_name} in {container_name}")
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c", f"/opt/bitnami/kafka/bin/kafka-topics.sh --create --topic {topic_name} --bootstrap-server {container_name}:9092"])
        logging.info(output)
        return code == 0

    def produce_message(self, container_name: str, topic_name: str, message: str, message_key: str|None = None):
        logging.info(f"Sending {message} to {container_name}:{topic_name}")
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c", f"/opt/bitnami/kafka/bin/kafka-console-producer.sh --topic {topic_name} --bootstrap-server {container_name}:9092 <<< '{message}'"])
        logging.info(output)
        return code == 0