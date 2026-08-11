// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/config.hpp>
#include <target_model/target.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <utility>

TEST_CASE("lwyi: get_target_config", "[lwyi]")
{
  std::map<target_model::Target, lwyi::Target_config> target_configs{
    {target_model::Target{"liba"}, lwyi::Target_config{true, {"pre_a", "pre_b"}}},
    {target_model::Target{"libb"}, lwyi::Target_config{false,{"pre_c", "pre_d"}}},
  };
  lwyi::Config config{std::move(target_configs)};

  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.skip_validation);
  CHECK(a_config.interface_include_prefixes.size() == 2);
  CHECK(a_config.interface_include_prefixes.contains("pre_a"));
  CHECK(a_config.interface_include_prefixes.contains("pre_b"));

  const auto& b_config = config.get_target_config(target_model::Target{"libb"});
  CHECK(!b_config.skip_validation);
  CHECK(b_config.interface_include_prefixes.size() == 2);
  CHECK(b_config.interface_include_prefixes.contains("pre_c"));
  CHECK(b_config.interface_include_prefixes.contains("pre_d"));

  // default config for unknown targets
  const auto& c_config = config.get_target_config(target_model::Target{"libc"});
  CHECK(!c_config.skip_validation);
  CHECK(c_config.interface_include_prefixes.empty());
}

TEST_CASE("lwyi: default config", "[lwyi]")
{
  lwyi::Config config;
  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(!a_config.skip_validation);
  CHECK(a_config.interface_include_prefixes.empty());
}
