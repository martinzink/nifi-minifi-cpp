/**
* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "KafkaClientControllerService.h"
#include "core/Resource.h"
#include "utils/gsl.h"
#include "utils/ParsingUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::kafka {

void KafkaClientControllerService::onEnable() {
  conf_ = {rd_kafka_conf_new(), utils::rd_kafka_conf_deleter()};
  constexpr std::string_view KAFKA_BROKER = "localhost:9092";  // TODO(mzink)
  topic_ = getProperty(TopicName.name) | utils::expect("Required property");
  consumer_group_ = getProperty(ConsumerGroup.name) | utils::expect("Required property");

  // TODO(mzink) error handling
  gsl_Assert(rd_kafka_conf_set(conf_.get(), "bootstrap.servers", KAFKA_BROKER.data(), nullptr, 0) == RD_KAFKA_CONF_OK);
  gsl_Assert(rd_kafka_conf_set(conf_.get(), "group.id", consumer_group_.data(), nullptr, 0) == RD_KAFKA_CONF_OK);
  gsl_Assert(rd_kafka_conf_set(conf_.get(), "enable.auto.commit", "false", nullptr, 0) == RD_KAFKA_CONF_OK);
  gsl_Assert(rd_kafka_conf_set(conf_.get(), "session.timeout.ms", "45000", nullptr, 0) == RD_KAFKA_CONF_OK);
  gsl_Assert(rd_kafka_conf_set(conf_.get(), "max.poll.interval.ms", "300000", nullptr, 0) == RD_KAFKA_CONF_OK);

  consumer_ = {rd_kafka_new(RD_KAFKA_CONSUMER, conf_.get(), nullptr, 0), utils::rd_kafka_consumer_deleter()};

  gsl_Assert(consumer_);

  partition_list_ = utils::rd_kafka_topic_partition_list_unique_ptr{rd_kafka_topic_partition_list_new(1)};

  gsl_Assert(partition_list_);

  rd_kafka_topic_partition_list_t* topics = rd_kafka_topic_partition_list_new(1);
  rd_kafka_topic_partition_list_add(topics, topic_.c_str(), RD_KAFKA_PARTITION_UA);
  gsl_Assert(rd_kafka_subscribe(consumer_.get(), topics) == RD_KAFKA_RESP_ERR_NO_ERROR);
}

std::shared_ptr<core::FlowFile> KafkaClientControllerService::poll(core::ProcessSession& session) {
  constexpr std::chrono::milliseconds poll_time_out = 100ms;
  utils::rd_kafka_message_unique_ptr message{rd_kafka_consumer_poll(consumer_.get(), poll_time_out.count())};
  if (message) {
    auto ff = session.create();
    ff->setAttribute("kafka.offset", std::to_string(message->offset));
    return ff;
  }
  return nullptr;
}

bool KafkaClientControllerService::commit(core::FlowFile& ff) {
  const auto offset = parsing::parseIntegral<uint64_t>(ff.getAttribute("kafka.offset") | utils::expect("Missing kafka.offset attribute")) | utils::expect("Invalid offset");
  auto partition = (partition_list_->elems[0]).partition;
  rd_kafka_topic_partition_list_add(partition_list_.get(), topic_.c_str(), partition)->offset = offset + 1;
  rd_kafka_commit(consumer_.get(), partition_list_.get(), 0);  // Synchronous commit
  return true;
}

REGISTER_RESOURCE(KafkaClientControllerService, ControllerService);
}  // namespace org::apache::nifi::minifi::kafka
