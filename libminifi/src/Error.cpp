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

#include "Error.h"

#include <cerrno>
#include "utils/gsl.h"

#ifdef WIN32
#include <windows.h>
#endif

namespace {

using org::apache::nifi::minifi::Errc;

struct MinifiErrCategory : std::error_category {
  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

const char* MinifiErrCategory::name() const noexcept {
  return "minifi";
}

std::string MinifiErrCategory::message(int ev) const {
  switch (static_cast<Errc>(ev)) {
    case Errc::FileOperationErr: {
      return "File operation error";
    }
    case Errc::FlowErr: {
      return "Flow error";
    }
    case Errc::ProcessorErr: {
      return "Processor error";
    }
    case Errc::ProcessSessionErr: {
      return "Process session error";
    }
    case Errc::ProcessScheduleErr: {
      return "Process schedule error";
    }
    case Errc::Site2SiteErr: {
      return "Site2Site error";
    }
    case Errc::GeneralErr: {
      return "General error";
    }
    case Errc::RegexErr: {
      return "Regex error";
    }
    case Errc::RepositoryErr: {
      return "Repository error";
    }
    case Errc::StreamError: {
      return "Stream error";
    }
  }
  return "(unrecognized error)";
}

const MinifiErrCategory minifiErrCategory {};

}  // namespace

namespace org::apache::nifi::minifi {

std::error_code make_error_code(Errc e) {
  return {static_cast<int>(e), minifiErrCategory};
}


std::error_code getLastOsError() {
#ifdef WIN32
  return {gsl::narrow<int>(GetLastError()), std::system_category()};
#else
  return {gsl::narrow<int>(errno), std::generic_category()};
#endif
}

}  // namespace org::apache::nifi::minifi
