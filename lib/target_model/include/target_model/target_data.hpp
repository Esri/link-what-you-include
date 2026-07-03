// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <target_model/target.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace target_model
{
struct Target_data
{
  std::unordered_set<std::filesystem::path> interface_headers;
  std::unordered_set<std::filesystem::path> interface_include_directories;
  std::unordered_set<std::string> interface_include_prefixes;
  std::unordered_set<Target> interface_dependencies;
  std::unordered_set<Target> dependencies;
  std::unordered_set<std::filesystem::path> sources;
  std::unordered_set<std::filesystem::path> headers;
  std::unordered_set<std::filesystem::path> verify_interface_header_sets_sources;
};

bool is_interface_header(const Target_data& target_data,
                         const std::filesystem::path& filename);
bool is_private_source(const Target_data& target_data, const std::filesystem::path& filename);
} // namespace target_model
