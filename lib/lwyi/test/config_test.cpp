// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/config.hpp>

#include <target_model/target.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <utility>

TEST_CASE("lwyi: get_target_config", "[lwyi]")
{
  std::map<target_model::Target, lwyi::Target_config> target_configs{
    {target_model::Target{"liba"},
     lwyi::Target_config{{"pre_a", "pre_b"},
                         {target_model::Target{"libq"}, target_model::Target{"libr"}},
                         {target_model::Target{"libs"}, target_model::Target{"libt"}},
                         false,
                         false,
                         true,
                         false}},
    {target_model::Target{"libb"},
     lwyi::Target_config{{"pre_c", "pre_d"}, {}, {}, false, false, false, true}}};
  lwyi::Config config{{}, std::move(target_configs)};

  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.interface_include_prefixes.size() == 2);
  CHECK(a_config.interface_include_prefixes.contains("pre_a"));
  CHECK(a_config.interface_include_prefixes.contains("pre_b"));
  CHECK(a_config.allow_includes_set.size() == 2);
  CHECK(a_config.allow_includes_set.contains(target_model::Target{"libq"}));
  CHECK(a_config.allow_includes_set.contains(target_model::Target{"libr"}));
  CHECK(a_config.allow_links_set.size() == 2);
  CHECK(a_config.allow_links_set.contains(target_model::Target{"libs"}));
  CHECK(a_config.allow_links_set.contains(target_model::Target{"libt"}));
  CHECK(a_config.allow_includes == false);
  CHECK(a_config.allow_links == false);
  CHECK(a_config.interface_allow_includes == true);
  CHECK(a_config.interface_allow_links == false);

  const auto& b_config = config.get_target_config(target_model::Target{"libb"});
  CHECK(b_config.interface_include_prefixes.size() == 2);
  CHECK(b_config.interface_include_prefixes.contains("pre_c"));
  CHECK(b_config.interface_include_prefixes.contains("pre_d"));
  CHECK(b_config.allow_includes_set.empty());
  CHECK(b_config.allow_links_set.empty());
  CHECK(b_config.allow_includes == false);
  CHECK(b_config.allow_links == false);
  CHECK(b_config.interface_allow_includes == false);
  CHECK(b_config.interface_allow_links == true);

  // default config for unknown targets
  const auto& c_config = config.get_target_config(target_model::Target{"libc"});
  CHECK(c_config.interface_include_prefixes.empty());
  CHECK(c_config.allow_includes_set.empty());
  CHECK(c_config.allow_links_set.empty());
  CHECK(c_config.allow_includes == false);
  CHECK(c_config.allow_links == false);
  CHECK(c_config.interface_allow_includes == false);
  CHECK(c_config.interface_allow_links == false);
}

TEST_CASE("lwyi: default config", "[lwyi]")
{
  lwyi::Config config;
  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.interface_include_prefixes.empty());
  CHECK(a_config.allow_includes_set.empty());
  CHECK(a_config.allow_links_set.empty());
  CHECK(a_config.allow_includes == false);
  CHECK(a_config.allow_links == false);
  CHECK(a_config.interface_allow_includes == false);
  CHECK(a_config.interface_allow_links == false);
}
