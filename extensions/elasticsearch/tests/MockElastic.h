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

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <CivetServer.h>
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch::test {

class MockElasticHandler : public CivetHandler {
 public:
  explicit MockElasticHandler(std::function<void(const struct mg_request_info *request_info)>& assertions) : assertions_(assertions) {
  }

  bool handlePost(CivetServer*, struct mg_connection *conn) override {
    const struct mg_request_info* req_info = mg_get_request_info(conn);
    assertions_(req_info);
    return handlePostImpl(conn);
  }
 protected:
  virtual bool handlePostImpl(struct mg_connection *conn) = 0;
  std::function<void(const struct mg_request_info *request_info)>& assertions_;
};

class BulkElasticHandler : public MockElasticHandler {
 public:
  explicit BulkElasticHandler(std::function<void(const struct mg_request_info *request_info)>& assertions) : MockElasticHandler(assertions) {}
 protected:
  bool handlePostImpl(struct mg_connection* conn) override {
    std::string request;
    request.reserve(2048);
    mg_read(conn, request.data(), 2048);

    std::vector<std::string> lines = utils::StringUtils::splitRemovingEmpty(request.data(), "\n");
    rapidjson::Document response{rapidjson::kObjectType};
    response.AddMember("took", 30, response.GetAllocator());
    response.AddMember("errors", false, response.GetAllocator());
    response.AddMember("items", rapidjson::kArrayType, response.GetAllocator());
    auto& items = response["items"];
    for (const auto& line : lines) {
      if (!line.starts_with("{\"index") && !line.starts_with("{\"create") && !line.starts_with("{\"update") && !line.starts_with("{\"delete"))
        continue;
      rapidjson::Value item{rapidjson::kObjectType};
      item.AddMember("_index", "test", response.GetAllocator());
      item.AddMember("_id", "1", response.GetAllocator());
      item.AddMember("result", "created", response.GetAllocator());
      items.PushBack(item, response.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    response.Accept(writer);

    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    mg_printf(conn, "Content-length: %lu", buffer.GetSize());
    mg_printf(conn, "\r\n\r\n");
    mg_printf(conn, "%s", buffer.GetString());
    return true;
  }
};

class MockElastic {
  struct CivetLibrary{
    CivetLibrary() {
      if (getCounter()++ == 0) {
        mg_init_library(0);
      }
    }
    ~CivetLibrary() {
      if (--getCounter() == 0) {
        mg_exit_library();
      }
    }
   private:
    static std::atomic<int>& getCounter() {
      static std::atomic<int> counter{0};
      return counter;
    }
  };

 public:
  explicit MockElastic(std::string port) : port_(std::move(port)) {
    std::vector<std::string> options;
    options.emplace_back("listening_ports");
    options.emplace_back(port_);
    server_ = std::make_unique<CivetServer>(options, &callbacks_, &logger_);
    MockElasticHandler* raw_collector_handler = new BulkElasticHandler(assertions_);
    server_->addHandler("/_bulk", raw_collector_handler);
    handlers_.emplace_back(raw_collector_handler);
  }

  [[nodiscard]] const std::string& getPort() const {
    return port_;
  }

  void setAssertions(std::function<void(const struct mg_request_info *request_info)> assertions) {
    assertions_ = std::move(assertions);
  }

 private:
  CivetLibrary lib_;
  std::string port_;
  std::unique_ptr<CivetServer> server_;
  std::vector<std::unique_ptr<MockElasticHandler>> handlers_;
  CivetCallbacks callbacks_;
  std::function<void(const struct mg_request_info *request_info)> assertions_{};
  std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger> logger_ = org::apache::nifi::minifi::core::logging::LoggerFactory<MockElastic>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
