// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>

#include <filesystem>
#include <flat_map>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace target_model
{
class Target_model
{
public:
  explicit Target_model(std::map<Target, Target_data> target_to_target_data);

  [[nodiscard]] std::string validate() const;

  [[nodiscard]] std::optional<std::reference_wrapper<const Target_data>> get_target_data(
    const Target& target) const;

  [[nodiscard]] std::optional<Target> map_header_to_target(
    const std::filesystem::path& header) const;

  void set_interface_include_prefixes(const Target& target,
                                      const std::set<std::string>& prefixes);

  void for_each_target(const std::function<void(const Target&, const Target_data&)>& visitor) const;

  [[nodiscard]] Target_model create_pruned(const std::vector<Target>& targets) const;

private:
  std::flat_map<Target, Target_data> target_to_target_data_;
  std::flat_map<std::filesystem::path, size_t> header_to_index_;
  std::vector<std::pair<std::filesystem::path, size_t>> directory_to_index_;
};
} // namespace target_model
