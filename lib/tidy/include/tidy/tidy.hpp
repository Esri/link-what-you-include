// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <target_model/target.hpp>

#include <cstdint>
#include <vector>

namespace target_model
{
class Target_model;
} // namespace target_model

namespace tidy
{
struct Config;

enum class Dag_diagnostic_type : uint8_t
{
  added_to_cluster,
  removed_from_cluster,
  new_cluster,
  forbidden_dependency
};

struct Dag_diagnostic
{
  Dag_diagnostic_type type;
  std::vector<target_model::Target> targets;
};

auto tidy(const Config& config, const target_model::Target_model& target_model)
  -> std::vector<Dag_diagnostic>;
} // namespace tidy
