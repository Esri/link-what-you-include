// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <tidy/tidy.hpp>

#include <lwyi/strongly_connected_dependencies.hpp>
#include <src/cluster_diff.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>
#include <tidy/config.hpp>

#include <cassert>
#include <cstddef>
#include <functional>
#include <optional>
#include <unordered_set>
#include <vector>

namespace tidy
{
std::vector<Dag_diagnostic> tidy(const Config& config,
                                 const target_model::Target_model& target_model)
{
  std::vector<Dag_diagnostic> diagnostics;

  // Check allowed_clusters
  const auto& allowed_clusters = config.allowed_clusters;
  const auto components = lwyi::compute_strongly_connected_dependencies(target_model);

  const auto result = cluster_diff(allowed_clusters, components);

  assert(allowed_clusters.size() <= result.size());

  for (size_t i = 0; i < allowed_clusters.size(); ++i)
  {
    const auto& diff = result[i];
    if (!diff.left_only.empty())
    {
      diagnostics.push_back({Dag_diagnostic_type::removed_from_cluster, diff.left_only});
    }
    if (!diff.right_only.empty())
    {
      diagnostics.push_back({Dag_diagnostic_type::added_to_cluster, diff.right_only});
    }
  }
  for (size_t i = allowed_clusters.size(); i < result.size(); ++i)
  {
    const auto& diff = result[i];
    assert(diff.left_only.empty());
    assert(!diff.right_only.empty());
    diagnostics.push_back({Dag_diagnostic_type::new_cluster, diff.right_only});
  }

  // Check forbidden_dependencies
  for (const auto& [target, forbidden_dependencies] : config.forbidden_dependencies)
  {
    auto target_data = target_model.get_target_data(target);
    if (!target_data.has_value())
    {
      continue;
    }

    for (const auto& forbidden_dependency : forbidden_dependencies)
    {
      if (0 < target_data->get().dependencies.count(forbidden_dependency))
      {
        diagnostics.push_back(
          {Dag_diagnostic_type::forbidden_dependency, {target, forbidden_dependency}});
      }
    }
  }

  return diagnostics;
}
} // namespace tidy
