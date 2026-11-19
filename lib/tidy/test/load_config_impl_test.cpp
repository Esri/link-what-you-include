// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/load_config_impl.hpp>

#include <target_model/target.hpp>
#include <tidy/config.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

TEST_CASE("tidy: load a valid config", "[tidy]")
{
  GIVEN("a valid config file")
  {
    const char* json = R"===({
      "forbidden_dependencies": {
        "liba": ["libx", "liby"],
        "libb": ["libz"]
      },
      "allowed_clusters": [
        [
          "libq",
          "libr"
        ],
        [
          "libs",
          "libt",
          "libu"
        ]
      ]
      })===";

    WHEN("load_config_impl is called")
    {
      auto result = tidy::load_config_impl(json);

      THEN("a valid config is returned")
      {
        REQUIRE(result.has_value());

        const auto& config = result.value();
        CHECK(config.forbidden_dependencies.at(target_model::Target{"liba"}) ==
              std::set<target_model::Target>{target_model::Target{"libx"},
                                             target_model::Target{"liby"}});
        CHECK(config.forbidden_dependencies.at(target_model::Target{"libb"}) ==
              std::set<target_model::Target>{target_model::Target{"libz"}});
        CHECK(config.allowed_clusters.at(0) ==
              std::set<target_model::Target>{target_model::Target{"libq"},
                                             target_model::Target{"libr"}});
        CHECK(config.allowed_clusters.at(1) ==
              std::set<target_model::Target>{target_model::Target{"libs"},
                                             target_model::Target{"libt"},
                                             target_model::Target{"libu"}});
      }
    }
  }
}

TEST_CASE("tidy: load_config fails for unknown root field", "[tidy]")
{
  const char* json = R"({
    "forbidden_dependencies": {},
    "allowed_clusters": [],
    "unknown": true
  })";
  auto result = tidy::load_config_impl(json);
  REQUIRE(!result.has_value());
  CHECK(result.error().find("unexpected field \"unknown\"") != std::string::npos);
}

TEST_CASE("tidy: load_config fails for invalid forbidden_dependencies type", "[tidy]")
{
  const char* json = R"({
    "forbidden_dependencies": [],
    "allowed_clusters": []
  })";
  auto result = tidy::load_config_impl(json);
  REQUIRE(!result.has_value());
  CHECK(result.error().find("forbidden_dependencies") != std::string::npos);
}

TEST_CASE("tidy: config fields can be reordered and empty collections are valid", "[tidy]")
{
  auto empty = tidy::load_config_impl(R"({"allowed_clusters": [], "forbidden_dependencies": {}})");
  REQUIRE(empty);
  CHECK(empty->allowed_clusters.empty());
  CHECK(empty->forbidden_dependencies.empty());

  auto reordered = tidy::load_config_impl(R"({
    "allowed_clusters": [["liba", "liba", "libb"], []],
    "forbidden_dependencies": {"liba": ["libb", "libb"], "libb": []}
  })");
  REQUIRE(reordered);
  CHECK(reordered->allowed_clusters ==
        std::vector<std::set<target_model::Target>>{
          {target_model::Target{"liba"}, target_model::Target{"libb"}}, {}});
  CHECK(reordered->forbidden_dependencies.at(target_model::Target{"liba"}) ==
        std::set<target_model::Target>{target_model::Target{"libb"}});
  CHECK(reordered->forbidden_dependencies.at(target_model::Target{"libb"}).empty());
}

TEST_CASE("tidy: config collections remain required", "[tidy]")
{
  auto missing_dependencies = tidy::load_config_impl(R"({"allowed_clusters": []})");
  REQUIRE_FALSE(missing_dependencies);
  CHECK(missing_dependencies.error() == "missing required field \"forbidden_dependencies\"");

  auto missing_clusters = tidy::load_config_impl(R"({"forbidden_dependencies": {}})");
  REQUIRE_FALSE(missing_clusters);
  CHECK(missing_clusters.error() == "missing required field \"allowed_clusters\"");
}

TEST_CASE("tidy: config errors identify nested values", "[tidy]")
{
  auto dependency = tidy::load_config_impl(R"({
    "forbidden_dependencies": {"liba": ["libb", 12]}, "allowed_clusters": []
  })");
  REQUIRE_FALSE(dependency);
  CHECK(dependency.error() == "\"liba\": element 1: expected string");

  auto cluster = tidy::load_config_impl(R"({
    "forbidden_dependencies": {}, "allowed_clusters": [["liba"], [false]]
  })");
  REQUIRE_FALSE(cluster);
  CHECK(cluster.error() == "field \"allowed_clusters\": element 1: element 0: expected string");
}

TEST_CASE("tidy: config rejects invalid collections and duplicate keys", "[tidy]")
{
  const std::vector<std::string_view> invalid{
    "null", "[]", "{", R"({"forbidden_dependencies": null, "allowed_clusters": []})",
    R"({"forbidden_dependencies": {"lib": null}, "allowed_clusters": []})",
    R"({"forbidden_dependencies": {"lib": "wrong"}, "allowed_clusters": []})",
    R"({"forbidden_dependencies": {}, "allowed_clusters": null})",
    R"({"forbidden_dependencies": {}, "allowed_clusters": {}})",
    R"({"forbidden_dependencies": {}, "allowed_clusters": ["wrong"]})",
    R"({"forbidden_dependencies": {"lib": [], "lib": []}, "allowed_clusters": []})",
    R"({"forbidden_dependencies": {}, "allowed_clusters": [], "allowed_clusters": []})"};
  for (auto text : invalid)
  {
    CAPTURE(text);
    CHECK_FALSE(tidy::load_config_impl(text));
  }
}
