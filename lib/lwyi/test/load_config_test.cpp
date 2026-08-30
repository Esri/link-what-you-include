// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/load_config_impl.hpp>
#include <target_model/target.hpp>

#include <catch2/catch_test_macros.hpp>
#include <simdjson.h>
#include <catch2/catch_message.hpp>

#include <cstring>
#include <set>
#include <string>

TEST_CASE("lwyi: load_config loads global and target configs", "[lwyi]")
{
  const char* json = R"({
      "pedantic": true,
      "targets": {
        "liba": {
          "allow_includes": ["libq", "libr"],
          "allow_links": ["libs", "libt"],
          "interface_allow_includes": true,
          "interface_allow_links": true,
          "interface_include_prefixes": ["foo", "bar"]
        },
        "libb": {
          "allow_includes": false,
          "allow_links": true,
          "interface_allow_links": true,
          "interface_include_prefixes": []
        }
      }
    })";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(result.has_value());

  const auto& config = result.value();

  const auto& global_config = config.get_global_config();
  CHECK(global_config.pedantic == true);

  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.interface_include_prefixes.size() == 2);
  CHECK(a_config.interface_include_prefixes.contains("foo"));
  CHECK(a_config.interface_include_prefixes.contains("bar"));
  CHECK(a_config.allow_includes_set.size() == 2);
  CHECK(a_config.allow_includes_set.contains(target_model::Target{"libq"}));
  CHECK(a_config.allow_includes_set.contains(target_model::Target{"libr"}));
  CHECK(a_config.allow_links_set.size() == 2);
  CHECK(a_config.allow_links_set.contains(target_model::Target{"libs"}));
  CHECK(a_config.allow_links_set.contains(target_model::Target{"libt"}));
  CHECK(a_config.allow_includes == false);
  CHECK(a_config.allow_links == false);
  CHECK(a_config.interface_allow_includes == true);
  CHECK(a_config.interface_allow_links == true);

  const auto& b_config = config.get_target_config(target_model::Target{"libb"});
  CHECK(b_config.interface_include_prefixes.empty());
  CHECK(b_config.allow_includes_set.empty());
  CHECK(b_config.allow_links_set.empty());
  CHECK(b_config.allow_includes == false);
  CHECK(b_config.allow_links == true);
  CHECK(b_config.interface_allow_includes == false);
  CHECK(b_config.interface_allow_links == true);
}

TEST_CASE("lwyi: load_config ignore", "[lwyi]")
{
  const char* json = R"({
      "targets": {
        "liba": {
          "ignore": true
        }
      }
    })";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(result.has_value());

  const auto& config = result.value();
  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.interface_include_prefixes.empty() == true);
  CHECK(a_config.allow_includes_set.empty() == true);
  CHECK(a_config.allow_links_set.empty() == true);
  CHECK(a_config.allow_includes == true);
  CHECK(a_config.allow_links == true);
  CHECK(a_config.interface_allow_includes == true);
  CHECK(a_config.interface_allow_links == true);
}

TEST_CASE("lwyi: load_config succeeds when targets is absent", "[lwyi]")
{
  const char* json = "{}";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(result.has_value());
}

TEST_CASE("lwyi: load_config defaults missing fields", "[lwyi]")
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

  const auto& global_config = config.get_global_config();
  CHECK(global_config.pedantic == false);

  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.interface_include_prefixes.empty());
  CHECK(a_config.allow_includes_set.empty());
  CHECK(a_config.allow_links_set.empty());
  CHECK(a_config.allow_includes == false);
  CHECK(a_config.allow_links == false);
  CHECK(a_config.interface_allow_includes == false);
  CHECK(a_config.interface_allow_links == false);
}

TEST_CASE("lwyi: load_config fails for invalid interface_allow_links type", "[lwyi]")
{
  const char* json = R"({
      "targets": {
        "liba": {
          "interface_allow_links": "true"
        }
      }
    })";
  simdjson::padded_string raw_config(json, std::strlen(json));

  auto result = lwyi::load_config_impl(raw_config);
  REQUIRE(!result.has_value());
  CHECK(result.error().find("interface_allow_links") != std::string::npos);
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
  REQUIRE(result.has_value() == false);
}

TEST_CASE("lwyi: load_config fails for conflicting ignore and allow fields", "[lwyi]")
{
  auto check_load_error = [](const char* json)
  {
    INFO(json);
    simdjson::padded_string raw_config(json, std::strlen(json));
    auto result = lwyi::load_config_impl(raw_config);
    REQUIRE(!result.has_value());
    INFO(result.error());
    CHECK(result.error().find("have conflicting values") != std::string::npos);
  };
  auto check_load_succeeds = [](const char* json)
  {
    INFO(json);
    simdjson::padded_string raw_config(json, std::strlen(json));
    auto result = lwyi::load_config_impl(raw_config);
    REQUIRE(result.has_value());
  };

  // allow_includes
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_includes": false
        }
      }
    })");
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_includes": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_includes": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_includes": false
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_includes": ["libq", "libr"]
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_includes": ["libq", "libr"]
        }
      }
    })");
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_includes": []
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_includes": []
        }
      }
    })");

  // allow_links
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_links": false
        }
      }
    })");
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_links": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_links": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_links": false
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_links": ["libq", "libr"]
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_links": ["libq", "libr"]
        }
      }
    })");
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "allow_links": []
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "allow_links": []
        }
      }
    })");

  // interface_allow_includes
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "interface_allow_includes": false
        }
      }
    })");
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "interface_allow_includes": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "interface_allow_includes": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "interface_allow_includes": false
        }
      }
    })");

  // interface_allow_links
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "interface_allow_links": false
        }
      }
    })");
  check_load_succeeds(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "interface_allow_links": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": false,
          "interface_allow_links": true
        }
      }
    })");
  check_load_error(R"({
      "targets": {
        "liba": {
          "ignore": true,
          "interface_allow_links": false
        }
      }
    })");
}
