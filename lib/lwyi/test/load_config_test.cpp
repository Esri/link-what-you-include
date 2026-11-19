// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/load_config_impl.hpp>

#include <target_model/target.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <format>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

TEST_CASE("lwyi: load_config loads global and target configs", "[lwyi]")
{
  auto result = lwyi::load_config_impl(R"({
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
  })");
  REQUIRE(result);
  CHECK(result->pedantic);

  const auto& liba = result->get_target_config(target_model::Target{"liba"});
  CHECK(liba.interface_include_prefixes == std::set<std::string>{"bar", "foo"});
  CHECK(liba.allow_includes ==
        lwyi::Allowance{std::set<target_model::Target>{{"libq"}, {"libr"}}});
  CHECK(liba.allow_links == lwyi::Allowance{std::set<target_model::Target>{{"libs"}, {"libt"}}});
  CHECK(liba.interface_allow_includes);
  CHECK(liba.interface_allow_links);

  const auto& libb = result->get_target_config(target_model::Target{"libb"});
  CHECK(libb.interface_include_prefixes.empty());
  CHECK(libb.allow_includes == lwyi::Allowance{false});
  CHECK(libb.allow_links.allows_all());
  CHECK_FALSE(libb.interface_allow_includes);
  CHECK(libb.interface_allow_links);
}

TEST_CASE("lwyi: load_config defaults optional fields", "[lwyi]")
{
  auto empty = lwyi::load_config_impl("{}");
  REQUIRE(empty);
  CHECK_FALSE(empty->pedantic);

  auto target = lwyi::load_config_impl(R"({"targets":{"liba":{}}})");
  REQUIRE(target);
  const auto& config = target->get_target_config(target_model::Target{"liba"});
  CHECK(config.interface_include_prefixes.empty());
  CHECK(config.allow_includes == lwyi::Allowance{});
  CHECK(config.allow_links == lwyi::Allowance{});
  CHECK_FALSE(config.interface_allow_includes);
  CHECK_FALSE(config.interface_allow_links);
}

TEST_CASE("lwyi: load_config stores ignore independently", "[lwyi]")
{
  auto result = lwyi::load_config_impl(R"({"targets":{"liba":{"ignore":true}}})");
  REQUIRE(result);
  const auto& config = result->get_target_config(target_model::Target{"liba"});
  CHECK(config.ignore);
  CHECK(config.allow_includes == lwyi::Allowance{});
  CHECK(config.allow_links == lwyi::Allowance{});
  CHECK_FALSE(config.interface_allow_includes);
  CHECK_FALSE(config.interface_allow_links);
  CHECK(config.skips_validation());

  // ignore: false is the default, so the allow fields apply as usual.
  auto not_ignored = lwyi::load_config_impl(
    R"({"targets":{"liba":{"ignore":false,"allow_includes":true,"interface_allow_links":true}}})");
  REQUIRE(not_ignored);
  const auto& liba = not_ignored->get_target_config(target_model::Target{"liba"});
  CHECK_FALSE(liba.ignore);
  CHECK(liba.allow_includes.allows_all());
  CHECK(liba.interface_allow_links);

  // interface_include_prefixes still matters for an ignored target.
  CHECK(lwyi::load_config_impl(
          R"({"targets":{"liba":{"ignore":true,"interface_include_prefixes":["a"]}}})")
          .has_value());
}

TEST_CASE("lwyi: load_config rejects allow fields that ignore makes redundant", "[lwyi]")
{
  const std::vector<std::pair<std::string_view, std::string_view>> redundant{
    {"allow_includes", "true"},
    {"allow_includes", R"(["libq"])"},
    {"allow_links", "true"},
    {"allow_links", R"(["libq"])"},
    {"interface_allow_includes", "true"},
    {"interface_allow_links", "true"}};
  for (const auto& [field, value] : redundant)
  {
    CAPTURE(field, value);
    auto result = lwyi::load_config_impl(R"({"targets":{"liba":{"ignore":true,")" +
                                         std::string(field) + R"(":)" + std::string(value) +
                                         "}}}");
    REQUIRE_FALSE(result);
    CHECK(result.error() ==
          std::format("\"liba\": {} cannot be set when ignore is true", field));
  }

  // Values equal to the defaults are indistinguishable from omitting the field.
  for (std::string_view value : {"false", "[]"})
  {
    CAPTURE(value);
    CHECK(lwyi::load_config_impl(
            R"({"targets":{"liba":{"ignore":true,"allow_includes":)" + std::string(value) + "}}}")
            .has_value());
  }
}

TEST_CASE("lwyi: load_config reports invalid fields", "[lwyi]")
{
  auto result = lwyi::load_config_impl(R"({
    "targets": {"lib/a~b": {"interface_include_prefixes": ["valid", 42]}}
  })");
  REQUIRE_FALSE(result);
  CHECK(result.error() ==
        "\"lib/a~b\": field \"interface_include_prefixes\": element 1: expected string");

  auto named = lwyi::load_config_impl(R"({
    "targets": {"liba": {"allow_links": null}}
  })");
  REQUIRE_FALSE(named);
  CHECK(named.error() == "\"liba\": field \"allow_links\": expected boolean or array");
}

TEST_CASE("lwyi: load_config rejects malformed and unknown fields", "[lwyi]")
{
  const std::vector<std::string_view> invalid{
    "{", "null", "[]", R"({"targets":null})", R"({"targets":[]})",
    R"({"targets":{"liba":null}})", R"({"targets":{"liba":{"ignore":null}}})",
    R"({"targets":{"liba":{"interface_include_prefixes":[true]}}})",
    R"({"targets":{"liba":{"allow_includes":[true]}}})",
    R"({"targets":{"liba":{"interface_allow_links":"true"}}})",
    R"({"targets":{},"targets":{}})", R"({"targets":{"liba":{},"liba":{}}})",
    R"({"targets":{"liba":{"ignore":true,"ignore":false}}})",
    R"({"unknown":true})", R"({"targets":{"liba":{"unknown":true}}})"};
  for (auto text : invalid)
  {
    auto result = lwyi::load_config_impl(text);
    REQUIRE_FALSE(result);
  }
}
