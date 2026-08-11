// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <tidy/tidy.hpp>

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>
#include <tidy/config.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <unordered_set>
#include <utility>
#include <vector>

TEST_CASE("tidy: tidy", "[tidy]")
{
  GIVEN("a dag check config and a target data that match")
  {
    tidy::Config config;
    config.allowed_clusters = {{{"liba"}, {"libb"}}, //
                               {{"libc"}, {"libd"}, {"libe"}}};
    config.forbidden_dependencies[{"libd"}] = {{"libc"}};

    target_model::Target_data liba_target_data;
    target_model::Target_data libb_target_data;
    target_model::Target_data libc_target_data;
    target_model::Target_data libd_target_data;
    target_model::Target_data libe_target_data;
    target_model::Target_data libf_target_data;
    target_model::Target_data libg_target_data;
    liba_target_data.dependencies = {{"libb"}};
    libb_target_data.dependencies = {{"liba"}, {"libf"}};
    libc_target_data.dependencies = {{"libd"}};
    libd_target_data.dependencies = {{"libe"}};
    libe_target_data.dependencies = {{"libc"}, {"libf"}};
    libf_target_data.dependencies = {{"libg"}};

    auto make_target_model = [&]() -> target_model::Target_model
    {
      std::map<target_model::Target, target_model::Target_data> target_to_target_data{
        {{"liba"}, liba_target_data},
        {{"libb"}, libb_target_data},
        {{"libc"}, libc_target_data},
        {{"libd"}, libd_target_data},
        {{"libe"}, libe_target_data},
        {{"libf"}, libf_target_data},
        {{"libg"}, libg_target_data}};
      return target_model::Target_model{std::move(target_to_target_data)};
    };

    WHEN("tidy is run")
    {
      auto target_model = make_target_model();

      auto diagnostics = tidy::tidy(config, target_model);

      THEN("No diagnostics are reported")
      {
        CHECK(diagnostics.empty());
      }
    }

    WHEN("tidy is run after a dependency is added that increases the size of a cluster")
    {
      libf_target_data.dependencies.insert({"libc"});
      auto target_model = make_target_model();

      auto diagnostics = tidy::tidy(config, target_model);

      THEN("one diagnostic is reported")
      {
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].type == tidy::Dag_diagnostic_type::added_to_cluster);
        CHECK(diagnostics[0].targets == std::vector<target_model::Target>{{"libf"}});
      }
    }

    WHEN("tidy is run after a dependency is changed that decreases the size of a cluster")
    {
      libc_target_data.dependencies.clear();
      libc_target_data.dependencies.insert({"libe"});
      auto target_model = make_target_model();

      auto diagnostics = tidy::tidy(config, target_model);

      THEN("one diagnostic is reported")
      {
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].type == tidy::Dag_diagnostic_type::removed_from_cluster);
        CHECK(diagnostics[0].targets == std::vector<target_model::Target>{{"libd"}});
      }
    }

    WHEN("tidy is run after a dependency is changed that adds a new cluster")
    {
      libg_target_data.dependencies.insert({"libf"});
      auto target_model = make_target_model();

      auto diagnostics = tidy::tidy(config, target_model);

      THEN("one diagnostic is reported")
      {
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].type == tidy::Dag_diagnostic_type::new_cluster);
        CHECK(diagnostics[0].targets ==
              std::vector<target_model::Target>{{"libf"}, {"libg"}});
      }
    }

    WHEN("tidy is run after a forbidden dependency is added")
    {
      libd_target_data.dependencies.insert({"libc"});
      auto target_model = make_target_model();

      auto diagnostics = tidy::tidy(config, target_model);

      THEN("one diagnostic is reported")
      {
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].type == tidy::Dag_diagnostic_type::forbidden_dependency);
        CHECK(diagnostics[0].targets ==
              std::vector<target_model::Target>{{"libd"}, {"libc"}});
      }
    }
  }
}
