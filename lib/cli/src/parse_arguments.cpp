// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cli/parse_arguments.hpp>

#include <cli/command_options.hpp>
#include <message/message.hpp>
#include <util/arg_parser.hpp>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli
{
namespace
{
constexpr std::string_view usage_string = R"(Usage:
  {0} [options]

Possible options:
  -h, --help                Print this help message.
  --color [WHEN]            Enable colored output. WHEN is 'never', 'always', or
                            'auto'. Plain --color means --color auto.
                            Default is --color auto.
  -v, --verbose             Show verbose output.
  --debug                   Show internal debug output.

  -d, --binary_dir DIR      Path to the directory with input files. Default is
                            the current directory.
  -c, --config FILE         Path to lwyi config file. Default is lwyi-config.json
                            in the build tree when present.
  -t, --targets TARGETS...  Limit analysis to the given targets.
  -j, --parallel COUNT      Number of threads used to process source files.
                            Default depends on system.

  --tool TOOL [OPTIONS...]  Run a tool. All subsequent arguments are passed to
                            the tool. This is undocumented and serves as a place
                            holder for future features.)";

struct Options
{
  bool help{false};
  std::optional<std::string> color_output;
  bool verbose{false};
  bool debug{false};
  std::string_view binary_dir;
  std::string_view config_file;
  uint32_t num_threads{0};
  std::vector<std::string_view> targets;
  std::vector<std::string_view> sources;
  std::vector<std::string_view> tool_command;
};

constexpr auto parser = util::arg_parser<Options>()
                          .arg("-h", "--help", &Options::help)
                          .arg("--color", &Options::color_output)
                          .arg("-v", "--verbose", &Options::verbose)
                          .arg("--debug", &Options::debug)
                          .arg("-d", "--binary_dir", &Options::binary_dir)
                          .arg("-c", "--config", &Options::config_file)
                          .arg("-t", "--targets", &Options::targets)
                          .arg("-j", "--parallel", &Options::num_threads)
                          .terminal_arg("--tool", &Options::tool_command);

std::string usage(std::string_view name)
{
  return std::format(usage_string, name);
}

message::Message_level get_message_level(const Options& options)
{
  if (options.debug)
  {
    return message::Message_level::debug;
  }
  if (options.verbose)
  {
    return message::Message_level::verbose;
  }

  return message::Message_level::normal;
}

bool is_set_in_environment(const char* env_var)
{
  auto* v = std::getenv(env_var);
  return v && std::string_view(v) != "0";
}
} // namespace

std::expected<Command_options, std::string> parse_arguments(int argc, const char* const* argv)
{
  assert(0 < argc);
  assert(argv != nullptr);

  const std::string_view name = *argv;

  auto result = parser.parse(std::next(argv, 1), std::next(argv, argc));

  if (!result.has_value())
  {
    return std::unexpected(result.error() + "\n" + usage(name) + "\n");
  }
  if (result.value().help)
  {
    return std::unexpected(usage(name));
  }

  auto& options = result.value();

  // The default color is automatic unless the NO_COLOR environment variable is set
  auto color_output = is_set_in_environment("NO_COLOR") ? message::Color_output::never
                                                        : message::Color_output::automatic;

  // The command line option overrides the default color
  if (options.color_output.has_value())
  {
    if (*options.color_output == "auto" || options.color_output->empty())
    {
      color_output = message::Color_output::automatic;
    }
    else if (*options.color_output == "always")
    {
      color_output = message::Color_output::always;
    }
    else if (*options.color_output == "never")
    {
      color_output = message::Color_output::never;
    }
    else
    {
      return std::unexpected(
        std::format("invalid color {}\n{}\n", *options.color_output, usage(name)));
    }
  }

  // The FORCE_COLOR environment variable overrides everything
  if (is_set_in_environment("FORCE_COLOR"))
  {
    color_output = message::Color_output::always;
  }

  return Command_options{options.binary_dir,
                         options.config_file,
                         std::move(options.targets),
                         std::move(options.tool_command),
                         color_output,
                         get_message_level(options),
                         options.num_threads};
}
} // namespace cli
