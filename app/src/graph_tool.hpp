// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string_view>
#include <vector>

namespace target_model
{
struct Target;
class Target_model;
} // namespace target_model

int graph_tool(const target_model::Target_model& target_model,
               const std::vector<target_model::Target>& selected_targets,
               const std::vector<std::string_view>& args);
