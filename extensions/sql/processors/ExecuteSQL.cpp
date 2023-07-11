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

#include "ExecuteSQL.h"

#include <string>
#include <utility>

#include "io/StreamPipe.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "Exception.h"

namespace org::apache::nifi::minifi::processors {

ExecuteSQL::ExecuteSQL(std::string name, const utils::Identifier& uuid)
  : SQLProcessor(std::move(name), uuid, core::logging::LoggerFactory<ExecuteSQL>::getLogger(uuid)) {
}

void ExecuteSQL::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ExecuteSQL::processOnSchedule(core::ProcessContext& context) {
  context.getProperty(OutputFormat, output_format_);

  max_rows_ = [&] {
    uint64_t max_rows = 0;
    context.getProperty(MaxRowsPerFlowFile, max_rows);
    return gsl::narrow<size_t>(max_rows);
  }();
}

void ExecuteSQL::processOnTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto input_flow_file = session.get();

  std::string query;
  if (!context.getProperty(SQLSelectQuery, query, input_flow_file)) {
    if (!input_flow_file) {
      throw Exception(PROCESSOR_EXCEPTION,
                      "No incoming FlowFile and the \"" + std::string{SQLSelectQuery.name} + "\" processor property is not specified");
    }
    logger_->log_debug("Using the contents of the flow file as the SQL statement");
    query = to_string(session.readBuffer(input_flow_file));
  }
  if (query.empty()) {
    logger_->log_error("Empty sql statement");
    if (input_flow_file) {
      session.transfer(input_flow_file, Failure);
      return;
    }
    throw Exception(PROCESSOR_EXCEPTION, "Empty SQL statement");
  }

  std::unique_ptr<sql::Rowset> row_set;
  try {
    row_set = connection_->prepareStatement(query)->execute(collectArguments(input_flow_file));
  } catch (const sql::StatementError& ex) {
    logger_->log_error("Error while executing sql statement: %s", ex.what());
    session.transfer(input_flow_file, Failure);
    return;
  }

  sql::JSONSQLWriter json_writer{output_format_ == flow_file_source::OutputType::JSONPretty};
  FlowFileGenerator flow_file_creator{session, json_writer};
  sql::SQLRowsetProcessor sql_rowset_processor(std::move(row_set), {json_writer, flow_file_creator});

  // Process rowset.
  while (size_t row_count = sql_rowset_processor.process(max_rows_)) {
    auto new_file = flow_file_creator.getLastFlowFile();
    gsl_Expects(new_file);
    new_file->addAttribute(ResultRowCount.name, std::to_string(row_count));
    if (input_flow_file) {
      new_file->addAttribute(InputFlowFileUuid.name, input_flow_file->getUUIDStr());
    }
  }

  // transfer flow files
  if (input_flow_file) {
    session.remove(input_flow_file);
  }
  for (const auto& new_file : flow_file_creator.getFlowFiles()) {
    session.transfer(new_file, Success);
  }
}

REGISTER_RESOURCE(ExecuteSQL, Processor);

}  // namespace org::apache::nifi::minifi::processors
