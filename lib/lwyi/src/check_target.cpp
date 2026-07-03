// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/check_target.hpp>

#include <lwyi/dependency_visibility.hpp>
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
    auto dep = target_model.map_header_to_target(include.path);
    if (dep.has_value())
    {
      deps[*dep].push_back(include);
    }
  }
  return deps;
}

struct Visibility
{
  Dependency_visibility linked_visibility{Dependency_visibility::none};
  Dependency_visibility included_visibility{Dependency_visibility::none};
};
} // namespace

std::vector<LWYI_error> check_target(const target_model::Target_model& target_model,
                                     [[maybe_unused]] const target_model::Target& target,
                                     const target_model::Target_data& target_data,
                                     const scanner::Intransitive_includes& target_includes)

{
  std::map<target_model::Target, Visibility> visibility_map;

  // filter linked dependencies to only include targets that have target data
  for (const auto& dep : target_data.interface_dependencies)
  {
    if (target_model.get_target_data(dep).has_value())
    {
      visibility_map[dep].linked_visibility |= Dependency_visibility::interface_scope;
    }
  }
  for (const auto& dep : target_data.dependencies)
  {
    if (target_model.get_target_data(dep).has_value())
    {
      visibility_map[dep].linked_visibility |= Dependency_visibility::private_scope;
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
    visibility_map[dep].included_visibility |= Dependency_visibility::interface_scope;
  }
  for (const auto& pair : included_deps_map)
  {
    const auto& dep = pair.first;
    visibility_map[dep].included_visibility |= Dependency_visibility::private_scope;
  }

  std::vector<LWYI_error> errors;
  for (const auto& [dep, visibility] : visibility_map)
  {
    if (visibility.linked_visibility == visibility.included_visibility)
    {
      continue;
    }
    LWYI_error error{dep, visibility.linked_visibility, visibility.included_visibility, {}};
    if (!!(visibility.included_visibility & Dependency_visibility::interface_scope))
    {
      auto it = included_interface_deps_map.find(dep);
      assert(it != included_interface_deps_map.end());
      error.sample_includes = it->second;
    }
    if (!!(visibility.included_visibility & Dependency_visibility::private_scope))
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
