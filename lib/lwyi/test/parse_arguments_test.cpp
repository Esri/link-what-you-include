// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/parse_arguments.hpp>

#include <lwyi/command_options.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#include <numeric>
#include <string_view>
#include <vector>

auto to_string(std::vector<const char*> args)
{
  return fmt::format("command line: {}\n",
                     std::accumulate(args.begin(),
                                     args.end(),
                                     std::string{},
                                     [](const auto& lhs, const auto& rhs)
                                     {
                                       return fmt::format("{} {}", lhs, rhs);
                                     }));
}

TEST_CASE("lwyi: parse_arguments for help", "[lwyi]")
{
  std::vector<std::vector<const char*>> args_list{
    {"exe_name", "-h"},
    {"exe_name", "--help"},
  };
  for (const auto& args : args_list)
  {
    INFO(to_string(args));
    const auto argc = static_cast<int>(args.size());
    const auto argv = args.data();
    auto result = lwyi::parse_arguments(argc, argv);
    REQUIRE(!result.has_value());

    const auto& usage = result.error();
    CAPTURE(usage);

    CHECK(usage.find("Usage:") != std::string::npos);
    CHECK(usage.find("exe_name") != std::string::npos);
    CHECK(usage.find("-h") != std::string::npos);
    CHECK(usage.find("--help") != std::string::npos);
  }
}

TEST_CASE("lwyi: parse_arguments for targets", "[lwyi]")
{
  std::vector<std::vector<const char*>> args_list{
    {"exe_name", "-d", "some/dir", "-t", "one", "two", "three"},
    {"exe_name", "-d", "some/dir", "--targets", "one", "two", "three"},
    {"exe_name", "-t", "one", "two", "three", "-d", "some/dir"},
    {"exe_name", "--targets", "one", "two", "three", "-d", "some/dir"},
  };
  for (const auto& args : args_list)
  {
    INFO(to_string(args));

    const auto argc = static_cast<int>(args.size());
    const auto argv = args.data();
    auto result = lwyi::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.binary_dir == "some/dir");
    REQUIRE(options.targets.size() == 3U);
    CHECK(options.targets[0] == "one");
    CHECK(options.targets[1] == "two");
    CHECK(options.targets[2] == "three");
  }
}

TEST_CASE("lwyi: parse_arguments for tool", "[lwyi]")
{
  std::vector<const char*>
    args{"exe_name", "-d", "some/dir", "--tool", "one", "-z", "two", "--yep", "three"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();
  auto result = lwyi::parse_arguments(argc, argv);
  REQUIRE(result.has_value());

  const auto& options = result.value();
  CHECK(options.binary_dir == "some/dir");

  std::vector<std::string_view> expected{"one", "-z", "two", "--yep", "three"};
  CHECK(options.tool_command == expected);
}
