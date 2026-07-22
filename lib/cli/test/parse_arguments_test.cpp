// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cli/parse_arguments.hpp>

#include <cli/command_options.hpp>
#include <message/message.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/interfaces/catch_interfaces_reporter.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <cassert>
#include <cstdint>
#include <format>
#include <numeric>
#include <string_view>
#include <vector>

// This is different than <cstdlib>. Needed for unsetenv.
#include <stdlib.h> // NOLINT(modernize-deprecated-headers)

namespace
{
void unset_env(const char* var)
{
  [[maybe_unused]] auto ret =
#ifdef _WIN32
    _putenv_s(var, "");
#else
    unsetenv(var);
#endif
  assert(ret == 0);
}

void set_env(const char* var, const char* value)
{
  [[maybe_unused]] auto ret =
#ifdef _WIN32
    _putenv_s(var, value);
#else
    setenv(var, value, 1);
#endif
  assert(ret == 0);
}

// Ensure that the environment variables that affect command line parsing are unset at the start of each run through a
// test case.
class testRunListener : public Catch::EventListenerBase
{
public:
  using Catch::EventListenerBase::EventListenerBase;
  void testCasePartialStarting(const Catch::TestCaseInfo&, uint64_t) override
  {
    unset_env("NO_COLOR");
    unset_env("FORCE_COLOR");
  }
};
} // namespace

CATCH_REGISTER_LISTENER(testRunListener)

auto to_string(std::vector<const char*> args)
{
  return std::format("command line: {}\n",
                     std::accumulate(args.begin(),
                                     args.end(),
                                     std::string{},
                                     [](const auto& lhs, const auto& rhs)
                                     {
                                       return std::format("{} {}", lhs, rhs);
                                     }));
}

TEST_CASE("cli: parse_arguments for help", "[lwyi]")
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
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(!result.has_value());

    const auto& usage = result.error();
    CAPTURE(usage);

    CHECK(usage.find("Usage:") != std::string::npos);
    CHECK(usage.find("exe_name") != std::string::npos);
    CHECK(usage.find("-h") != std::string::npos);
    CHECK(usage.find("--help") != std::string::npos);
  }
}

TEST_CASE("cli: parse_arguments for targets", "[lwyi]")
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
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.binary_dir == "some/dir");
    REQUIRE(options.targets.size() == 3U);
    CHECK(options.targets[0] == "one");
    CHECK(options.targets[1] == "two");
    CHECK(options.targets[2] == "three");
  }
}

TEST_CASE("cli: parse_arguments for tool", "[lwyi]")
{
  std::vector<const char*>
    args{"exe_name", "-d", "some/dir", "--tool", "one", "-z", "two", "--yep", "three"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();
  auto result = cli::parse_arguments(argc, argv);
  REQUIRE(result.has_value());

  const auto& options = result.value();
  CHECK(options.binary_dir == "some/dir");

  std::vector<std::string_view> expected{"one", "-z", "two", "--yep", "three"};
  CHECK(options.tool_command == expected);
}

TEST_CASE("cli: parse_arguments rejects invalid attached integer arg", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "-jasdf"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();
  auto result = cli::parse_arguments(argc, argv);
  REQUIRE(!result.has_value());
}

TEST_CASE("cli: parse_arguments for config", "[lwyi]")
{
  std::vector<std::vector<const char*>> args_list{
    {"exe_name", "-c", "lwyi-config.json", "-d", "some/dir"},
    {"exe_name", "--config", "lwyi-config.json", "-d", "some/dir"},
  };
  for (const auto& args : args_list)
  {
    INFO(to_string(args));

    const auto argc = static_cast<int>(args.size());
    const auto argv = args.data();
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.config_file == "lwyi-config.json");
    CHECK(options.binary_dir == "some/dir");
  }
}

TEST_CASE("cli: parse_arguments when no --color", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "-d", "some/dir"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();

  SECTION("Empty environment")
  {
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::automatic);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::never);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("FORCE_COLOR environment")
  {
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR and FORCE_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
}

TEST_CASE("cli: parse_arguments when --color", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "--color", "-d", "some/dir"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();

  SECTION("Empty environment")
  {
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::automatic);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::automatic);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("FORCE_COLOR environment")
  {
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR and FORCE_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
}

TEST_CASE("cli: parse_arguments when --color auto", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "--color", "auto", "-d", "some/dir"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();

  SECTION("Empty environment")
  {
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::automatic);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::automatic);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("FORCE_COLOR environment")
  {
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR and FORCE_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
}

TEST_CASE("cli: parse_arguments when color never", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "--color", "never", "-d", "some/dir"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();

  SECTION("Empty environment")
  {
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::never);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::never);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("FORCE_COLOR environment")
  {
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR and FORCE_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
}

TEST_CASE("cli: parse_arguments when color always", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "--color", "always", "-d", "some/dir"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();

  SECTION("Empty environment")
  {
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("FORCE_COLOR environment")
  {
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
  SECTION("NO_COLOR and FORCE_COLOR environment")
  {
    set_env("NO_COLOR", "1");
    set_env("FORCE_COLOR", "1");
    auto result = cli::parse_arguments(argc, argv);
    REQUIRE(result.has_value());

    const auto& options = result.value();
    CHECK(options.color_output == message::Color_output::always);
    CHECK(options.binary_dir == "some/dir");
  }
}

TEST_CASE("cli: parse_arguments for verbose", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "--verbose", "-d", "some/dir"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();
  auto result = cli::parse_arguments(argc, argv);
  REQUIRE(result.has_value());

  const auto& options = result.value();
  CHECK(options.message_level == message::Message_level::verbose);
  CHECK(options.binary_dir == "some/dir");
}

TEST_CASE("cli: parse_arguments for debug", "[lwyi]")
{
  std::vector<const char*> args{"exe_name", "--debug", "-d", "some/dir"};
  INFO(to_string(args));

  const auto argc = static_cast<int>(args.size());
  const auto argv = args.data();
  auto result = cli::parse_arguments(argc, argv);
  REQUIRE(result.has_value());

  const auto& options = result.value();
  CHECK(options.message_level == message::Message_level::debug);
  CHECK(options.binary_dir == "some/dir");
}
