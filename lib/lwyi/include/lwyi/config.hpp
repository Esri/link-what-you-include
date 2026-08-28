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
struct Global_config
{
  bool pedantic{false};
};

struct Target_config
{
  std::set<std::string> interface_include_prefixes;
  std::set<target_model::Target> allow_includes_set;
  std::set<target_model::Target> allow_links_set;
  bool allow_includes{false};
  bool allow_links{false};
  bool interface_allow_includes{false};
  bool interface_allow_links{false};
};

class Config
{
public:
  Config();
  Config(const Global_config& global_config,
         std::map<target_model::Target, Target_config> target_configs);

  [[nodiscard]] const Global_config& get_global_config() const;
  [[nodiscard]] const Target_config& get_target_config(const target_model::Target& target) const;

  void for_each_non_default_target_config(
    const std::function<void(const target_model::Target&, const Target_config&)>& visitor) const;

private:
  Global_config global_config_;
  std::flat_map<target_model::Target, Target_config> target_configs_;
};
} // namespace lwyi
