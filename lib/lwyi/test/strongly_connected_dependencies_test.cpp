// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/strongly_connected_dependencies.hpp>

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

TEST_CASE("lwyi: strongly_connected_dependencies", "[lwyi]")
{
  //    a
  //   ↗ ↘
  //  c ← b
  //       ↘
  //        d

  target_model::Target_data liba_target_data;
  liba_target_data.dependencies = {{"b"}};

  target_model::Target_data libb_target_data;
  libb_target_data.dependencies = {{"c"}, {"d"}};

  target_model::Target_data libc_target_data;
  libc_target_data.dependencies = {{"a"}};

  target_model::Target_data libd_target_data;
  libd_target_data.dependencies = {};

  std::map<target_model::Target, target_model::Target_data> target_to_target_data{
    {{"a"}, liba_target_data},
    {{"b"}, libb_target_data},
    {{"c"}, libc_target_data},
    {{"d"}, libd_target_data},
  };
  target_model::Target_model target_model{std::move(target_to_target_data)};

  auto dependency_groups = lwyi::compute_strongly_connected_dependencies(target_model);

  REQUIRE(dependency_groups.size() == 1);

  CHECK(dependency_groups[0] == std::set<target_model::Target>{{"a"}, {"b"}, {"c"}});
}
