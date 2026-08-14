// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/run_lwyi_on_target.hpp>

#include <lwyi/check_target.hpp>
#include <lwyi/dependency_scope.hpp>
#include <message/message.hpp>
#include <scanner/include.hpp>
#include <scanner/scan.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>

#include <filesystem>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
std::string_view scope_to_string(lwyi::Dependency_scope scope)
{
  switch (scope)
  {
    case lwyi::Dependency_scope::none:
      return "none";
    case lwyi::Dependency_scope::private_scope:
      return "PRIVATE";
    case lwyi::Dependency_scope::interface_scope:
      return "INTERFACE";
    case lwyi::Dependency_scope::public_scope:
      return "PUBLIC";
  }

  std::unreachable();
}

std::string describe_linked_scope(lwyi::Dependency_scope scope, std::string_view target_name)
{
  if (scope == lwyi::Dependency_scope::none)
  {
    return std::format("does not link to {}", target_name);
  }
  return std::format("links to {} with {} scope", target_name, scope_to_string(scope));
}

std::string describe_included_scope(lwyi::Dependency_scope scope)
{
  if (scope == lwyi::Dependency_scope::none)
  {
    return "not included";
  }
  return std::format("included with {} scope", scope_to_string(scope));
}
} // namespace

bool run_lwyi_on_target(const target_model::Target_model& target_model,
                        const std::filesystem::path& binary_dir,
                        const target_model::Target& target,
                        const target_model::Target_data& target_data,
                        unsigned int num_threads)
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
                                return error.linked_scope ==
                                         lwyi::Dependency_scope::public_scope &&
                                       error.included_scope ==
                                         lwyi::Dependency_scope::interface_scope;
                              }),
               errors.end());
#endif

  if (errors.empty())
  {
    message::status("ok",
                    "All included dependencies are linked correctly.",
                    message::Style::success);
    return true;
  }

  for (const auto& error : errors)
  {
    message::error("{} {} but it is {}.",
                   target.name,
                   describe_linked_scope(error.linked_scope, error.target.name),
                   describe_included_scope(error.included_scope));

    if (error.linked_location.has_value())
    {
      message::note("linked in {}:{}",
                    error.linked_location->file.string(),
                    error.linked_location->line);
    }

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
