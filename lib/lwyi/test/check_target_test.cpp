// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/check_target.hpp>

#include <lwyi/config.hpp>
#include <lwyi/dependency_scope.hpp>
#include <scanner/include.hpp>
#include <scanner/scan.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>
#include <print>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
void dump(const std::vector<lwyi::LWYI_error>& errors)
{
  auto to_string = [](lwyi::Dependency_scope scope) -> std::string_view
  {
    switch (scope)
    {
      case lwyi::Dependency_scope::private_scope:
        return "PRIVATE";
      case lwyi::Dependency_scope::interface_scope:
        return "INTERFACE";
      case lwyi::Dependency_scope::public_scope:
        return "PUBLIC";
      case lwyi::Dependency_scope::none:
        break;
    }

    return "none";
  };

  for (const auto& error : errors)
  {
    std::print("target:         {}\n", error.target.name);
    std::print("linked_scope:   {}\n", to_string(error.linked_scope));
    std::print("included_scope: {}\n", to_string(error.included_scope));

    for (const auto& include : error.sample_includes)
    {
      std::print("  {}\n", include.path.string());
      for (const auto& source_line : std::ranges::reverse_view(include.include_chain))
      {
        std::print("    included from {}:{}\n", source_line.source.string(), source_line.line);
      }
    }
  }
}
} // namespace

TEST_CASE("lwyi: check_target", "[lwyi]")
{
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

  GIVEN("a target model with a target that depends on other targets in all the various ways")
  {
    // target_link_libraries(libq
    //                       PUBLIC liba
    //                       PRIVATE libb
    //                       INTERFACE libc)

    target_model::Target_data liba_target_data;
    liba_target_data.interface_headers = {"/liba/include/one.h", "/liba/include/two.h"};
    liba_target_data.sources = {"/liba/src/one.cpp", "/liba/src/two.cpp"};

    target_model::Target_data libb_target_data;
    libb_target_data.interface_headers = {"/libb/include/one.h", "/libb/include/two.h"};
    libb_target_data.sources = {"/libb/src/one.cpp", "/libb/src/two.cpp"};

    target_model::Target_data libc_target_data;
    libc_target_data.interface_headers = {"/libc/include/one.h", "/libc/include/two.h"};
    libc_target_data.sources = {"/libc/src/one.cpp", "/libc/src/two.cpp"};

    target_model::Target_data libd_target_data;
    libd_target_data.interface_headers = {"/libd/include/one.h", "/libd/include/two.h"};
    libd_target_data.sources = {"/libd/src/one.cpp", "/libd/src/two.cpp"};

    target_model::Target_data libq_target_data;
    libq_target_data.interface_headers = {"/libq/include/one.h", "/libq/include/two.h"};
    libq_target_data.sources = {"/libq/src/one.cpp", "/libq/src/two.cpp"};
    libq_target_data.interface_dependencies = {{"liba"}, {"libc"}};
    libq_target_data.dependencies = {{"liba"}, {"libb"}};

    std::map<target_model::Target, target_model::Target_data> target_to_target_data{
      {{"liba"}, liba_target_data},
      {{"libb"}, libb_target_data},
      {{"libc"}, libc_target_data},
      {{"libd"}, libd_target_data},
      {{"libq"}, libq_target_data}};
    target_model::Target_model target_model{std::move(target_to_target_data)};

    const target_model::Target target{"libq"};

    AND_GIVEN("correct intransitive includes")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}}};
      intransitive_includes.includes = {{"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
                                        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that only include a PUBLIC dependency with PRIVATE scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}}};
      intransitive_includes.includes = {{"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
                                        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);

        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].target == target_model::Target{"liba"});
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::public_scope);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::private_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_links_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links_set.insert(target_model::Target{"liba"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_links")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_links")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"liba"}];
        target_config.interface_allow_links = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that only include a PUBLIC dependency with INTERFACE scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}}};
      intransitive_includes.includes = {
        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);

        THEN("no errors are returned because this type of overlinking is allowed when in non-pedantic mode")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a pedantic config")
      {
        auto errors = lwyi::check_target({{true}, {}},
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::public_scope);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::interface_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_links_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links_set.insert(target_model::Target{"liba"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_links")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_links")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"liba"}];
        target_config.interface_allow_links = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that include a PRIVATE dependency with PUBLIC scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libb/include/one.h", {{"/libq/include/two.h", 40U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}}};
      intransitive_includes.includes = {{"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
                                        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::private_scope);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::public_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_includes_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes_set.insert(target_model::Target{"libb"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libb"}];
        target_config.interface_allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that include a PRIVATE dependency with INTERFACE scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libb/include/one.h", {{"/libq/include/two.h", 40U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}}};
      intransitive_includes.includes = {
        {"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::private_scope);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::interface_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_links_set and allow_includes_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links_set.insert(target_model::Target{"libb"});
        target_config.allow_includes_set.insert(target_model::Target{"libb"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_links and allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links = true;
        target_config.allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_links and interface_allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libb"}];
        target_config.interface_allow_links = true;
        target_config.interface_allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that include an INTERFACE dependency with PUBLIC scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}}};
      intransitive_includes.includes = {
        {"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}},
        {"/libc/include/one.h", {{"/libq/src/one.cpp", 13U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::interface_scope);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::public_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_includes_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes_set.insert(target_model::Target{"libc"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_links")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libc"}];
        target_config.interface_allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that include an INTERFACE dependency with PRIVATE scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}}};
      intransitive_includes.includes = {
        {"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}},
        {"/libc/include/one.h", {{"/libq/src/one.cpp", 13U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::interface_scope);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::private_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_links_set and allow_includes_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links_set.insert(target_model::Target{"libc"});
        target_config.allow_includes_set.insert(target_model::Target{"libc"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_links and allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_links = true;
        target_config.allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_links and interface_allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libc"}];
        target_config.interface_allow_links = true;
        target_config.interface_allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that include a non-dependency with PRIVATE scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}}};
      intransitive_includes.includes = {
        {"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}},
        {"/libd/include/one.h", {{"/libd/src/two.cpp", 34U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::none);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::private_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_includes_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes_set.insert(target_model::Target{"libd"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libd"}];
        target_config.interface_allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that include a non-dependency with INTERFACE scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}},
        {"/libd/include/one.h", {{"/libq/include/one.h", 4U}}}};
      intransitive_includes.includes = {{"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
                                        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::none);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::interface_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_includes_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes_set.insert(target_model::Target{"libd"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libd"}];
        target_config.interface_allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
    AND_GIVEN("intransitive includes that include a non-dependency with PUBLIC scope")
    {
      scanner::Intransitive_includes intransitive_includes;
      intransitive_includes.interface_includes = {
        {"/liba/include/one.h", {{"/libq/include/one.h", 17U}}},
        {"/libc/include/one.h", {{"/libq/include/two.h", 42U}}},
        {"/libd/include/one.h", {{"/libq/include/one.h", 4U}}}};
      intransitive_includes.includes = {
        {"/liba/include/one.h", {{"/libq/src/one.cpp", 12U}}},
        {"/libb/include/one.h", {{"/libq/src/two.cpp", 34U}}},
        {"/libd/include/one.h", {{"/libd/src/two.cpp", 34U}}}};

      WHEN("check_target is called with a default config")
      {
        auto errors =
          lwyi::check_target({}, target_model, target, libq_target_data, intransitive_includes);
        THEN("an error is produced")
        {
          REQUIRE(errors.size() == 1);
          CHECK(errors[0].linked_scope == lwyi::Dependency_scope::none);
          CHECK(errors[0].included_scope == lwyi::Dependency_scope::public_scope);
        }
      }
      WHEN("check_target is called with a config that has allow_includes_set")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes_set.insert(target_model::Target{"libd"});
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libq"}];
        target_config.allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
      WHEN("check_target is called with a config that has interface_allow_includes")
      {
        std::map<target_model::Target, lwyi::Target_config> target_configs;
        auto& target_config = target_configs[target_model::Target{"libd"}];
        target_config.interface_allow_includes = true;
        lwyi::Config config({}, std::move(target_configs));
        auto errors = lwyi::check_target(config,
                                         target_model,
                                         target,
                                         libq_target_data,
                                         intransitive_includes);

        THEN("no errors are returned")
        {
          CHECK(errors.empty());
          if (!errors.empty())
          {
            dump(errors);
          }
        }
      }
    }
  }

  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}
