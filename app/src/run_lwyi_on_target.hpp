// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>

namespace target_model
{
struct Build_system_data;
struct Target;
struct Target_data;
class Target_model;
} // namespace target_model

bool run_lwyi_on_target(const target_model::Target_model& target_model,
                        const std::filesystem::path& binary_dir,
                        const target_model::Target& target,
                        const target_model::Target_data& target_data,
                        unsigned int num_threads);
