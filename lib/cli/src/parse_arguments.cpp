// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cli/parse_arguments.hpp>

#include <cli/command_options.hpp>
#include <message/message.hpp>
#include <util/arg_parser.hpp>

#include <cassert>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli
{
namespace
{
struct Options
{
  bool help{false};
  bool color_output{false};
  bool verbose{false};
  bool debug{false};
  std::string_view binary_dir;
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
                          .arg("-t", "--targets", &Options::targets)
                          .arg("-j", "--parallel", &Options::num_threads)
                          .terminal_arg("--tool", &Options::tool_command);

std::string usage(std::string_view name)
{
  return std::format(
    "Usage:\n"
    "  {} [options]\n\n"
    "Possible options:\n"
    "  -h, --help                Print this help message.\n"
    "  --color                   Enable colored terminal output.\n"
    "  -v, --verbose             Show verbose output.\n"
    "  --debug                   Show internal debug output.\n\n"
    "  -d, --binary_dir DIR      Path to the directory with input files. Default is\n"
    "                            the current directory.\n"
    "  -t, --targets TARGETS...  Limit analysis to the given targets.\n"
    "  -j, --parallel COUNT      Number of threads used to process source files.\n"
    "                            Default depends on system.\n\n"
    "  --tool TOOL [OPTIONS...]  Run a tool. All subsequent arguments are passed to\n"
    "                            the tool. This is undocumented and serves as a place\n"
    "                            holder for future features.",
    name);
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
  return Command_options{options.binary_dir,
                         std::move(options.targets),
                         std::move(options.tool_command),
                         options.color_output,
                         get_message_level(options),
                         options.num_threads};
}
} // namespace cli
