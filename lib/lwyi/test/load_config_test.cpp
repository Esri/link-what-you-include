// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/load_config_impl.hpp>

#include <target_model/target.hpp>

#include <catch2/catch_test_macros.hpp>

#include <simdjson.h>

#include <cstring>
#include <set>
#include <string>

TEST_CASE("lwyi: load_config loads target overrides", "[lwyi]")
{
  const char* json = R"({
      "targets": {
        "liba": {
          "skip_validation": true,
          "interface_skip_validation": true,
          "interface_include_prefixes": ["foo", "bar"]
        },
        "libb": {
          "skip_validation": false,
          "interface_skip_validation": false,
          "interface_include_prefixes": []
        }
      }
    })";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(result.has_value());

  const auto& config = result.value();

  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.skip_validation);
  CHECK(a_config.interface_skip_validation);
  CHECK(a_config.interface_include_prefixes == std::set<std::string>{"bar", "foo"});

  const auto& b_config = config.get_target_config(target_model::Target{"libb"});
  CHECK(!b_config.skip_validation);
  CHECK(!b_config.interface_skip_validation);
  CHECK(b_config.interface_include_prefixes.empty());
}

TEST_CASE("lwyi: load_config succeeds when targets is absent", "[lwyi]")
{
  const char* json = "{}";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(result.has_value());
}

TEST_CASE("lwyi: load_config defaults missing target fields", "[lwyi]")
{
  const char* json = R"({
      "targets": {
        "liba": {}
      }
    })";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(result.has_value());

  const auto& config = result.value();
  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(!a_config.skip_validation);
  CHECK(a_config.interface_include_prefixes.empty());
}

TEST_CASE("lwyi: load_config fails for invalid skip_validation type", "[lwyi]")
{
  const char* json = R"({
      "targets": {
        "liba": {
          "skip_validation": "true"
        }
      }
    })";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(!result.has_value());
  CHECK(result.error().find("skip_validation") != std::string::npos);
}

TEST_CASE("lwyi: load_config fails for invalid interface_include_prefixes type", "[lwyi]")
{
  const char* json = R"({
      "targets": {
        "liba": {
          "interface_include_prefixes": "foo"
        }
      }
    })";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(!result.has_value());
  CHECK(result.error().find("interface_include_prefixes") != std::string::npos);
}

TEST_CASE("lwyi: load_config fails for invalid JSON", "[lwyi]")
{
  const char* json = "{";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(!result.has_value());
}
