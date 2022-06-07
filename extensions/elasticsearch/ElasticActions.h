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


#pragma once
#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "PutElasticsearchJson.h"
#include "utils/expected.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

class ElasticAction {
 public:
  virtual std::string toString() const = 0;
  virtual ~ElasticAction() {}
};

class DeleteAction : public ElasticAction {
  std::string toString() const override {
    rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
    root.AddMember("delete", rapidjson::Value{ rapidjson::kObjectType }, root.GetAllocator());
    auto& operation_request = root["delete"];

    {
      auto index_json = rapidjson::Value(index_.data(), index_.size(), root.GetAllocator());
      operation_request.AddMember("_index", index_json, root.GetAllocator());
    }

    {
      auto id_json = rapidjson::Value(id_.data(), id_.size(), root.GetAllocator());
      operation_request.AddMember("_id", id_json, root.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    root.Accept(writer);

    return buffer.GetString();
  }

  static auto parse(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) -> nonstd::expected<DeleteAction, std::string> {
    auto index = context.getProperty(PutElasticsearchJson::Index, flow_file);
    if (!index)
      return nonstd::make_unexpected("Missing index");

    auto id = context.getProperty(PutElasticsearchJson::Id, flow_file);
    if (!id)
      return nonstd::make_unexpected("Missing Id");

    return DeleteAction(*index, *id);
  }

 protected:
  DeleteAction(std::string index, std::string id) : index_(std::move(index)), id_(std::move(id)) {}

  std::string index_;
  std::string id_;
};

class IndexAction : public ElasticAction {
 public:
  std::string toString() const override {
    return firstLine() + '\n' + secondLine();
  }

  static auto parse(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) -> nonstd::expected<std::unique_ptr<DeleteAction>, std::string> {
    auto index = context.getProperty(PutElasticsearchJson::Index, flow_file);
    if (!index)
      return nonstd::make_unexpected("Missing index");

    auto id = context.getProperty(PutElasticsearchJson::Id, flow_file);

    return IndexAction(*index, id, );
  }

 protected:
  IndexAction(std::string index, std::optional<std::string> id, rapidjson::Document&& fields) : index_(std::move(index)), id_(std::move(id)), fields_(std::move(fields)) {}

  std::string firstLine() const {
    rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
    root.AddMember("index", rapidjson::Value{ rapidjson::kObjectType }, root.GetAllocator());
    auto& operation_request = root["index"];

    {
      auto index_json = rapidjson::Value(index_.data(), index_.size(), root.GetAllocator());
      operation_request.AddMember("_index", index_json, root.GetAllocator());
    }

    if (id_) {
      auto id_json = rapidjson::Value(id_->data(), id_->size(), root.GetAllocator());
      operation_request.AddMember("_id", id_json, root.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    root.Accept(writer);

    return buffer.GetString();
  }

  std::string secondLine() const {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    fields_.Accept(writer);
    return buffer.GetString();
  }

  std::string index_;
  std::optional<std::string> id_;
  rapidjson::Document fields_;
};

static auto parse(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) -> nonstd::expected<std::unique_ptr<ElasticAction>, std::string> {
  auto action_type = context.getProperty(PutElasticsearchJson::IndexOperation, flow_file);
  if (!action_type)
    return nonstd::make_unexpected("Missing index operation");

  if (action_type == "delete")
    return DeleteAction::parse(context, flow_file);

  auto id = context.getProperty(PutElasticsearchJson::Id, flow_file);

  return IndexAction(*index, id, );
}


}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
