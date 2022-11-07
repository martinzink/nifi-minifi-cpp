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

#include <chrono>
#include <tuple>
#include <system_error>
#include <utility>

#include "asio/ssl.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/write.hpp"
#include "asio/steady_timer.hpp"
#include "asio/this_coro.hpp"
#include "asio/use_awaitable.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "asio/experimental/as_tuple.hpp"

namespace org::apache::nifi::minifi::utils::net {

constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(asio::use_awaitable);

using HandshakeType = asio::ssl::stream_base::handshake_type;
using TcpSocket = asio::ip::tcp::socket;
using SslSocket = asio::ssl::stream<asio::ip::tcp::socket>;

namespace detail {
#if defined(__GNUC__) && __GNUC__ < 11
// [coroutines] unexpected 'warning: statement has no effect [-Wunused-value]'
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96749
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#endif  // defined(__GNUC__) && __GNUC__ < 11
asio::awaitable<void> timeout(std::chrono::steady_clock::duration duration) {
  asio::steady_timer timer(co_await asio::this_coro::executor);  // NOLINT
  timer.expires_after(duration);
  co_await timer.async_wait(utils::net::use_nothrow_awaitable);
}
#if defined(__GNUC__) && __GNUC__ < 11
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) && __GNUC__ < 11
}  // namespace detail

template<class... Types>
asio::awaitable<std::tuple<std::error_code, Types...>> asyncOperationWithTimeout(asio::awaitable<std::tuple<std::error_code, Types...>> async_operation,
                                                                                 std::chrono::steady_clock::duration timeout_duration) {
  using asio::experimental::awaitable_operators::operator||;
  auto operation_result = co_await(std::move(async_operation) || detail::timeout(timeout_duration));
  if (operation_result.index() == 1) {
    std::tuple<std::error_code, Types...> result;
    std::get<0>(result) = asio::error::timed_out;
    co_return result;
  }
  co_return std::get<0>(operation_result);
}
}  // namespace org::apache::nifi::minifi::utils::net
