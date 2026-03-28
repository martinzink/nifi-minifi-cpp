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

#include <functional>
#include <memory>
#include <optional>
#include <cinttypes>
#include "minifi-cpp/utils/gsl.h"

#include "../../minifi-api/include/minifi-c/minifi-c.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::io {

class InputStream;
class OutputStream;

struct ReadWriteResult {
  int64_t bytes_written = 0;
  int64_t bytes_read = 0;
};

class IoResult {
public:
  IoResult() = delete;
  IoResult(const IoResult&) = delete;
  IoResult(IoResult&&) = delete;
  IoResult& operator=(IoResult&&) = delete;
  IoResult& operator=(const IoResult&) = delete;

  virtual ~IoResult() = default;

  static IoResult error() {
    return IoResult(nonstd::make_unexpected(MINIFI_IO_ERROR));
  }
  static IoResult cancelled() {
    return IoResult(nonstd::make_unexpected(MINIFI_IO_CANCEL));
  }

  static IoResult fromI64(int64_t i64_val) {
    if (i64_val < 0) {
      return IoResult(nonstd::make_unexpected(static_cast<MinifiIoStatus>(i64_val)));
    }
    return IoResult(gsl::narrow<uint64_t>(i64_val));
  }

private:
  explicit IoResult(nonstd::expected<uint64_t, MinifiIoStatus> result) : result_(std::move(result)) {}

  nonstd::expected<uint64_t, MinifiIoStatus> result_;
};

using ExpectedCallbackReturn = nonstd::expected<uint64_t, MinifiIoStatus>;

inline ExpectedCallbackReturn i64ToExpectedCallbackReturn(int64_t i64_val) {
  if (i64_val < 0) {
    return nonstd::make_unexpected(static_cast<MinifiIoStatus>(i64_val));
  }
  return gsl::narrow<uint64_t>(i64_val);
}

inline int64_t expectedCallbackReturnToI64(const nonstd::expected<uint64_t, MinifiIoStatus>& expected_val) {
  if (expected_val.has_value()) {
    return gsl::narrow<int64_t>(*expected_val);
  }
  return expected_val.error();
}

class StrictOutputStreamCallback {
public:
  template <typename Callable,
            typename ActualRet = std::invoke_result_t<Callable, const std::shared_ptr<io::OutputStream>&>>
  StrictOutputStreamCallback(Callable&& c) : func_(std::forward<Callable>(c)) {
    static_assert(std::is_same_v<ActualRet, ExpectedCallbackReturn>,
        "FATAL: Callback return type mismatch! You must return exactly nonstd::expected<uint64_t, MinifiIoStatus>. "
        "Implicit conversions (like bool or int to expected) are strictly forbidden here.");
  }

  ExpectedCallbackReturn operator()(const std::shared_ptr<io::OutputStream>& os) const {
    return func_(os);
  }

private:
  std::function<ExpectedCallbackReturn(const std::shared_ptr<io::OutputStream>&)> func_;
};

class StrictInputStreamCallback {
public:
  template <typename Callable,
            typename ActualRet = std::invoke_result_t<Callable, const std::shared_ptr<io::InputStream>&>>
  StrictInputStreamCallback(Callable&& c) : func_(std::forward<Callable>(c)) {
    static_assert(std::is_same_v<ActualRet, ExpectedCallbackReturn>,
        "FATAL: Callback return type mismatch! You must return exactly nonstd::expected<uint64_t, MinifiIoStatus>. "
        "Implicit conversions (like bool or int to expected) are strictly forbidden here.");
  }

  ExpectedCallbackReturn operator()(const std::shared_ptr<io::InputStream>& os) const {
    return func_(os);
  }

private:
  std::function<ExpectedCallbackReturn(const std::shared_ptr<io::InputStream>&)> func_;
};

using OutputStreamCallback = StrictOutputStreamCallback;
using InputStreamCallback = StrictInputStreamCallback;
using InputOutputStreamCallback = std::function<std::optional<ReadWriteResult>(const std::shared_ptr<InputStream>& input_stream, const std::shared_ptr<OutputStream>& output_stream)>;

}  // namespace org::apache::nifi::minifi::io
