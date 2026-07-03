// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace target_model
{
class Target_model
{
public:
  explicit Target_model(std::vector<std::pair<Target, Target_data>> target_to_target_data);

  std::string validate() const;

  std::optional<std::reference_wrapper<const Target_data>> get_target_data(
    const Target& target) const;

  std::optional<Target> map_header_to_target(const std::filesystem::path& header) const;

  void for_each_target(const std::function<void(const Target&, const Target_data&)>& visitor) const;

  Target_model create_pruned(const std::vector<Target>& targets) const;

private:
  using Element = std::pair<Target, Target_data>;
  std::vector<Element> target_to_target_data_;
  std::unordered_map<std::filesystem::path, const Element*> header_to_target_;
  std::vector<std::pair<std::filesystem::path, const Element*>> directory_to_target_;
};
} // namespace target_model
