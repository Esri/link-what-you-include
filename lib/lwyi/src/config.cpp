// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/config.hpp>

#include <target_model/target.hpp>

#include <flat_map>
#include <functional>
#include <iterator>
#include <map>
#include <utility>

namespace lwyi
{
Config::Config() = default;

Config::Config(const Global_config& global_config,
               std::map<target_model::Target, Target_config> target_configs)
: global_config_(global_config),
  target_configs_(std::sorted_unique, // NOLINT(misc-include-cleaner) false error
                  std::make_move_iterator(target_configs.begin()),
                  std::make_move_iterator(target_configs.end()))
{
}

const Global_config& Config::get_global_config() const
{
  return global_config_;
}

const Target_config& Config::get_target_config(const target_model::Target& target) const
{
  if (const auto it = target_configs_.find(target); it != target_configs_.end())
  {
    return it->second;
  }

  static Target_config default_target_config;
  return default_target_config;
}

void Config::for_each_non_default_target_config(
  const std::function<void(const target_model::Target&, const Target_config&)>& visitor) const
{
  for (const auto& pair : target_configs_)
  {
    visitor(pair.first, pair.second);
  }
}
} // namespace lwyi
