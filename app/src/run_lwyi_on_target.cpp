// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/run_lwyi_on_target.hpp>

#include <lwyi/check_target.hpp>
#include <lwyi/dependency_visibility.hpp>
#include <message/message.hpp>
#include <scanner/include.hpp>
#include <scanner/scan.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>

#include <format>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
auto describe_linked_visibility(lwyi::Dependency_visibility visibility,
                                std::string_view target_name) -> std::string
{
  switch (visibility)
  {
    case lwyi::Dependency_visibility::none:
      return std::format("does not link to {}", target_name);
    case lwyi::Dependency_visibility::private_scope:
      return std::format("links to {} with PRIVATE scope", target_name);
    case lwyi::Dependency_visibility::interface_scope:
      return std::format("links to {} with INTERFACE scope", target_name);
    case lwyi::Dependency_visibility::public_scope:
      return std::format("links to {} with PUBLIC scope", target_name);
  }

  return {};
}

auto describe_included_visibility(lwyi::Dependency_visibility visibility) -> std::string_view
{
  switch (visibility)
  {
    case lwyi::Dependency_visibility::none:
      return "not included";
    case lwyi::Dependency_visibility::private_scope:
      return "included with PRIVATE scope";
    case lwyi::Dependency_visibility::interface_scope:
      return "included with INTERFACE scope";
    case lwyi::Dependency_visibility::public_scope:
      return "included with PUBLIC scope";
  }

  return {};
}
} // namespace

auto run_lwyi_on_target(const target_model::Target_model& target_model,
                        const std::filesystem::path& binary_dir,
                        const target_model::Target& target,
                        const target_model::Target_data& target_data,
                        unsigned int num_threads) -> bool
{
  static scanner::Scanner scanner(num_threads);
  if (target_data.sources.empty() && target_data.verify_interface_header_sets_sources.empty())
  {
    message::note("No sources to scan. Skipping target.");
    return true;
  }

  auto eincludes = scanner.scan(binary_dir, target_data);
  if (!eincludes.has_value())
  {
    message::error_block(std::format("Failed to scan direct includes for {}", target.name),
                         eincludes.error());
    return false;
  }

  auto errors = lwyi::check_target(target_model, target, target_data, *eincludes);

  // TODO: consider enabling the following with a command line option
#if 0
  // special case: ignore linked PUBLIC but included INTERFACE errors
  errors.erase(std::remove_if(errors.begin(),
                              errors.end(),
                              [](const lwyi::LWYI_error& error)
                              {
                                return error.linked_visibility ==
                                         lwyi::Dependency_visibility::public_scope &&
                                       error.included_visibility ==
                                         lwyi::Dependency_visibility::interface_scope;
                              }),
               errors.end());
#endif

  if (errors.empty())
  {
    message::status(
        "ok", "All included dependencies are linked correctly.",
        message::Style::success);
    return true;
  }

  for (const auto& error : errors)
  {
    message::error("{} {} but it is {}.",
                   target.name,
                   describe_linked_visibility(error.linked_visibility, error.target.name),
                   describe_included_visibility(error.included_visibility));

    for (const auto& include : error.sample_includes)
    {
      message::note(include.path.string());
      for (const auto& source_line : std::ranges::reverse_view(include.include_chain))
      {
        message::print("  included from {}:{}", source_line.source.string(), source_line.line);
      }
    }
  }

  return false;
}
