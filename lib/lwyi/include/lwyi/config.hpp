// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <target_model/target.hpp>

#include <flat_map>
#include <functional>
#include <map>
#include <set>

namespace lwyi
{
struct Target_config
{
  bool skip_validation{false};
  std::set<std::string> interface_include_prefixes;
};

class Config
{
public:
  Config();
  explicit Config(std::map<target_model::Target, Target_config> target_configs);

  [[nodiscard]] const Target_config& get_target_config(const target_model::Target& target) const;

  void for_each_non_default_target_config(
    const std::function<void(const target_model::Target&, const Target_config&)>& visitor) const;

private:
  std::flat_map<target_model::Target, Target_config> target_configs_;
};
} // namespace lwyi
