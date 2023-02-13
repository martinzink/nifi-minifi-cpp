/**
 *
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

#include <string>

#include "../TestBase.h"
#include "../Catch.h"
#include "utils/net/DNS.h"
#include "utils/net/Socket.h"
#include "utils/StringUtils.h"
#include "../Utils.h"
#include "range/v3/algorithm/contains.hpp"

namespace utils = org::apache::nifi::minifi::utils;
namespace net = utils::net;

TEST_CASE("net::resolveHost", "[net][dns][utils][resolveHost]") {
  REQUIRE(net::sockaddr_ntop(net::resolveHost("127.0.0.1", "10080").value()->ai_addr) == "127.0.0.1");
  const auto localhost_address = net::sockaddr_ntop(net::resolveHost("localhost", "10080").value()->ai_addr);
  REQUIRE((utils::StringUtils::startsWith(localhost_address, "127") || localhost_address == "::1"));
}

TEST_CASE("net::reverseLookup", "[net][dns][reverseLookup]") {
  SECTION("dns.google") {
    nonstd::expected<std::vector<std::string>, std::error_code> dns_google_hostname;
    SECTION("IPv4") {
      dns_google_hostname = net::reverseLookup(asio::ip::address::from_string("8.8.8.8"));
    }
    SECTION("IPv6") {
      if (minifi::test::utils::isIPv6Disabled())
        return;
      dns_google_hostname = net::reverseLookup(asio::ip::address::from_string("2001:4860:4860::8888"));
    }
    REQUIRE(dns_google_hostname.has_value());
    CHECK(ranges::contains(*dns_google_hostname, "dns.google"));
  }

  SECTION("localhost") {
    nonstd::expected<std::vector<std::string>, std::error_code> localhost_hostname;
    SECTION("IPv4") {
      localhost_hostname = net::reverseLookup(asio::ip::address::from_string("127.0.0.1"));
    }
    SECTION("IPv6") {
      if (minifi::test::utils::isIPv6Disabled())
        return;
      localhost_hostname = net::reverseLookup(asio::ip::address::from_string("::1"));
    }
    REQUIRE(localhost_hostname.has_value());
    CHECK(ranges::contains(*localhost_hostname, "localhost"));
  }

  SECTION("Unresolvable address IPv4") {
    auto unresolvable_address = net::reverseLookup(asio::ip::address::from_string("192.0.2.0"));
    REQUIRE(unresolvable_address.has_value());
    CHECK(ranges::contains(*unresolvable_address, "192.0.2.0"));
  }

  SECTION("Unresolvable address IPv6") {
    if (minifi::test::utils::isIPv6Disabled())
      return;
    auto unresolvable_address = net::reverseLookup(asio::ip::address::from_string("2001:db8::"));
    REQUIRE(unresolvable_address.has_value());
    CHECK(ranges::contains(*unresolvable_address, "2001:db8::"));
  }
}
