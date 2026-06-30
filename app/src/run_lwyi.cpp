// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/run_lwyi.hpp>

#include <cli/command_options.hpp>
#include <message/message.hpp>
#include <src/run_lwyi_on_target.hpp>
#include <src/run_tool.hpp>
#include <target_model/target.hpp>
#include <target_model/target_model.hpp>
#include <target_model/target_model_loader.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace target_model
{
struct Target_data;
} // namespace target_model

auto run_lwyi(const cli::Command_options& options) -> std::expected<int, std::string>
{
  auto working_dir = std::filesystem::current_path();
  auto binary_dir = working_dir;
  if (!options.binary_dir.empty())
  {
    binary_dir = options.binary_dir;
    if (!std::filesystem::is_directory(binary_dir))
    {
      return std::unexpected(std::format("error: {} is not a directory", binary_dir.string()));
    }
  }

  const auto info_file = binary_dir / "link_what_you_include_info.json";
  message::heading("Build System");
  if (!std::filesystem::is_regular_file(info_file))
  {
    return std::unexpected(std::format("error: {} is not a file", info_file.string()));
  }

  message::info("Loading metadata from {}", info_file.string());

  auto loader = target_model::Target_model_loader::create();
  const auto load_result = loader->load_json(info_file);
  if (!load_result.has_value())
  {
    return std::unexpected(
      std::format("error: failed to load {}: {}", info_file.string(), load_result.error()));
  }
  const auto target_model = loader->make_target_model();

  std::vector<target_model::Target> selected_targets;
  selected_targets.reserve(options.targets.size());
  for (auto v : options.targets)
  {
    selected_targets.push_back({std::string(v)});
  }

  if (!options.tool_command.empty())
  {
    return run_tool(target_model, selected_targets, options.tool_command);
  }

  const auto num_threads = (0U < options.num_threads) ? options.num_threads
                                                      : std::thread::hardware_concurrency();
  message::info("Scanning with {} thread{}", num_threads, num_threads == 1 ? "" : "s");
  message::blank_line();

  bool success = true;
  bool first_target = true;
  if (selected_targets.empty())
  {
     target_model.for_each_target(
        [&](const target_model::Target& target, const target_model::Target_data& target_data)
      {
        if (!first_target)
        {
          message::blank_line();
        }
        first_target = false;

        message::heading("Target: {}", target.name);

        success &=
          run_lwyi_on_target(
            target_model, binary_dir, target, target_data, num_threads);
      });
  }
  else
  {
    for (const auto& target : selected_targets)
    {
      if (!first_target)
      {
        message::blank_line();
      }
      first_target = false;

      message::heading("Target: {}", target.name);

      auto otarget_data = target_model.get_target_data(target);
      if (!otarget_data.has_value())
      {
        message::error("No target named {} found", target.name);
        success = false;
        break;
      }

      success &=
        run_lwyi_on_target(target_model,
                           binary_dir,
                           target,
                           *otarget_data,
                           num_threads);
    }
  }

  if (!success)
  {
    return 1;
  }

  return 0;
}
