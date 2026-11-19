// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/config.hpp>

#include <target_model/target.hpp>

#include <catch2/catch_test_macros.hpp>

#include <flat_map>
#include <set>
#include <utility>

TEST_CASE("lwyi: get_target_config", "[lwyi]")
{
  std::flat_map<target_model::Target, lwyi::Target_config> target_configs{
    {target_model::Target{"liba"},
     lwyi::Target_config{
       .interface_include_prefixes = {"pre_a", "pre_b"},
       .allow_includes = lwyi::Allowance{std::set<target_model::Target>{
         target_model::Target{"libq"}, target_model::Target{"libr"}}},
       .allow_links = lwyi::Allowance{std::set<target_model::Target>{
         target_model::Target{"libs"}, target_model::Target{"libt"}}},
       .interface_allow_includes = true}},
    {target_model::Target{"libb"},
     lwyi::Target_config{.interface_include_prefixes = {"pre_c", "pre_d"},
                         .interface_allow_links = true}}};
  lwyi::Config config{.targets = std::move(target_configs)};

  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.interface_include_prefixes.size() == 2);
  CHECK(a_config.interface_include_prefixes.contains("pre_a"));
  CHECK(a_config.interface_include_prefixes.contains("pre_b"));
  CHECK(a_config.allow_includes.allows(target_model::Target{"libq"}));
  CHECK(a_config.allow_includes.allows(target_model::Target{"libr"}));
  CHECK_FALSE(a_config.allow_includes.allows(target_model::Target{"libs"}));
  CHECK(a_config.allow_links.allows(target_model::Target{"libs"}));
  CHECK(a_config.allow_links.allows(target_model::Target{"libt"}));
  CHECK_FALSE(a_config.allow_links.allows(target_model::Target{"libq"}));
  CHECK_FALSE(a_config.allow_includes.allows_all());
  CHECK_FALSE(a_config.allow_links.allows_all());
  CHECK(a_config.interface_allow_includes == true);
  CHECK(a_config.interface_allow_links == false);

  const auto& b_config = config.get_target_config(target_model::Target{"libb"});
  CHECK(b_config.interface_include_prefixes.size() == 2);
  CHECK(b_config.interface_include_prefixes.contains("pre_c"));
  CHECK(b_config.interface_include_prefixes.contains("pre_d"));
  CHECK(b_config.allow_includes == lwyi::Allowance{});
  CHECK(b_config.allow_links == lwyi::Allowance{});
  CHECK(b_config.interface_allow_includes == false);
  CHECK(b_config.interface_allow_links == true);

  // default config for unknown targets
  const auto& c_config = config.get_target_config(target_model::Target{"libc"});
  CHECK(c_config.interface_include_prefixes.empty());
  CHECK(c_config.allow_includes == lwyi::Allowance{});
  CHECK(c_config.allow_links == lwyi::Allowance{});
  CHECK(c_config.interface_allow_includes == false);
  CHECK(c_config.interface_allow_links == false);
}

TEST_CASE("lwyi: default config", "[lwyi]")
{
  lwyi::Config config;
  const auto& a_config = config.get_target_config(target_model::Target{"liba"});
  CHECK(a_config.interface_include_prefixes.empty());
  CHECK(a_config.allow_includes == lwyi::Allowance{});
  CHECK(a_config.allow_links == lwyi::Allowance{});
  CHECK(a_config.interface_allow_includes == false);
  CHECK(a_config.interface_allow_links == false);
}

TEST_CASE("lwyi: allowance mirrors a boolean or a target list", "[lwyi]")
{
  const target_model::Target liba{"liba"};
  const target_model::Target libb{"libb"};

  const lwyi::Allowance none;
  CHECK_FALSE(none.allows(liba));
  CHECK_FALSE(none.allows_all());

  const lwyi::Allowance all{true};
  CHECK(all.allows(liba));
  CHECK(all.allows_all());

  const lwyi::Allowance some{std::set<target_model::Target>{liba}};
  CHECK(some.allows(liba));
  CHECK_FALSE(some.allows(libb));
  CHECK_FALSE(some.allows_all());

  // An empty list allows nothing, exactly like false.
  CHECK(lwyi::Allowance{std::set<target_model::Target>{}} == lwyi::Allowance{false});
  CHECK(none == lwyi::Allowance{false});
  CHECK(some != all);
}

TEST_CASE("lwyi: target config rules account for ignore", "[lwyi]")
{
  const target_model::Target dep{"dep"};
  const lwyi::Target_config plain;
  const lwyi::Target_config ignored{.ignore = true};
  const lwyi::Target_config interface_allowed{.interface_allow_includes = true};

  CHECK_FALSE(plain.skips_validation());
  CHECK(ignored.skips_validation());
  CHECK(lwyi::Target_config{.allow_includes = lwyi::Allowance{true},
                            .allow_links = lwyi::Allowance{true}}
          .skips_validation());
  CHECK_FALSE(lwyi::Target_config{.allow_includes = lwyi::Allowance{true}}.skips_validation());

  CHECK_FALSE(plain.may_include_without_linking(dep, plain));
  CHECK_FALSE(plain.may_link_without_including(dep, plain));
  // An ignored target may use anything ...
  CHECK(ignored.may_include_without_linking(dep, plain));
  CHECK(ignored.may_link_without_including(dep, plain));
  // ... and anything may use an ignored target.
  CHECK(plain.may_include_without_linking(dep, ignored));
  CHECK(plain.may_link_without_including(dep, ignored));
  // interface_allow_* applies only to its own direction.
  CHECK(plain.may_include_without_linking(dep, interface_allowed));
  CHECK_FALSE(plain.may_link_without_including(dep, interface_allowed));
  // Target lists apply only to the listed targets.
  const lwyi::Target_config listed{
    .allow_links = lwyi::Allowance{std::set<target_model::Target>{dep}}};
  CHECK(listed.may_link_without_including(dep, plain));
  CHECK_FALSE(listed.may_link_without_including(target_model::Target{"other"}, plain));
  CHECK_FALSE(listed.may_include_without_linking(dep, plain));
}
