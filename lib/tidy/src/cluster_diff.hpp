// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <target_model/target.hpp>

#include <set>
#include <vector>

namespace tidy
{
struct Cluster_diff
{
  std::vector<target_model::Target> left_only;
  std::vector<target_model::Target> right_only;
};

std::vector<Cluster_diff> cluster_diff(const std::vector<std::set<target_model::Target>>& lhs,
                                       const std::vector<std::set<target_model::Target>>& rhs);
} // namespace tidy
