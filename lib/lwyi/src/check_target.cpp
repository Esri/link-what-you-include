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

#include <cassert>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace lwyi
{
namespace
{
std::map<target_model::Target, std::vector<scanner::Include>> collect_include_deps(
  const target_model::Target_model& target_model,
  const std::vector<scanner::Include>& includes)

{
  std::map<target_model::Target, std::vector<scanner::Include>> deps;
  for (const auto& include : includes)
  {
    auto targets = target_model.map_header_to_targets(include.path);
    if (!targets.empty())
    {
      // TODO: error if 1 < size
      deps[targets[0]].push_back(include);
    }
  }
  return deps;
}

struct Visibility
{
  Dependency_scope linked_scope{Dependency_scope::none};
  Dependency_scope included_scope{Dependency_scope::none};
};
} // namespace

std::vector<LWYI_error> check_target(const lwyi::Config& config,
                                     const target_model::Target_model& target_model,
                                     [[maybe_unused]] const target_model::Target& target,
                                     const target_model::Target_data& target_data,
                                     const scanner::Intransitive_includes& target_includes)
{
  static_cast<void>(config);
  std::map<target_model::Target, Visibility> visibility_map;

  // filter linked dependencies to only include targets that have target data
  for (const auto& dep : target_data.interface_dependencies)
  {
    if (target_model.get_target_data(dep).has_value())
    {
      visibility_map[dep].linked_scope |= Dependency_scope::interface_scope;
    }
  }
  for (const auto& dep : target_data.dependencies)
  {
    if (target_model.get_target_data(dep).has_value())
    {
      visibility_map[dep].linked_scope |= Dependency_scope::private_scope;
    }
  }

  // map the included headers to their targets and group them by the targets
  const std::map<target_model::Target, std::vector<scanner::Include>> included_interface_deps_map =
    collect_include_deps(target_model, target_includes.interface_includes);
  const std::map<target_model::Target, std::vector<scanner::Include>> included_deps_map =
    collect_include_deps(target_model, target_includes.includes);

  // isolate the included target dependencies
  for (const auto& pair : included_interface_deps_map)
  {
    const auto& dep = pair.first;
    visibility_map[dep].included_scope |= Dependency_scope::interface_scope;
  }
  for (const auto& pair : included_deps_map)
  {
    const auto& dep = pair.first;
    visibility_map[dep].included_scope |= Dependency_scope::private_scope;
  }

  std::vector<LWYI_error> errors;
  for (const auto& [dep, visibility] : visibility_map)
  {
    if (visibility.linked_scope == visibility.included_scope)
    {
      continue;
    }
    std::optional<target_model::Source_location> linked_location;
    if (auto it = target_data.dependency_locations.find(dep);
        it != target_data.dependency_locations.end())
    {
      linked_location = it->second;
    }

    LWYI_error error{dep,
                     visibility.linked_scope,
                     visibility.included_scope,
                     std::move(linked_location),
                     {}};
    if (!!(visibility.included_scope & Dependency_scope::interface_scope))
    {
      auto it = included_interface_deps_map.find(dep);
      assert(it != included_interface_deps_map.end());
      error.sample_includes = it->second;
    }
    if (!!(visibility.included_scope & Dependency_scope::private_scope))
    {
      auto it = included_deps_map.find(dep);
      assert(it != included_deps_map.end());
      error.sample_includes.insert(error.sample_includes.end(),
                                   it->second.begin(),
                                   it->second.end());
    }
    errors.push_back(std::move(error));
  }

  return errors;
}
} // namespace lwyi
