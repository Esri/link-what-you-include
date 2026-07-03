// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <set>
#include <vector>

namespace target_model
{
struct Target;
class Target_model;
} // namespace target_model

namespace lwyi
{
// Find non-trivial strongly connected sub-graphs of the dependency graph
std::vector<std::set<target_model::Target>> compute_strongly_connected_dependencies(
  const target_model::Target_model& target_model);
} // namespace lwyi
