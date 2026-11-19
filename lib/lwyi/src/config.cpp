// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/config.hpp>

#include <target_model/target.hpp>

#include <flat_map>
#include <set>
#include <utility>

namespace lwyi
{
Allowance::Allowance(bool all)
: all_(all)
{
}

Allowance::Allowance(std::set<target_model::Target> targets)
: targets_(std::move(targets))
{
}

bool Allowance::allows(const target_model::Target& target) const
{
  return all_ || targets_.contains(target);
}

bool Allowance::allows_all() const
{
  return all_;
}

bool Target_config::skips_validation() const
{
  return ignore || (allow_includes.allows_all() && allow_links.allows_all());
}

// An ignored dependency is treated as if it set both interface_allow_* fields:
// no other target is reported for how it uses an ignored target.
bool Target_config::may_include_without_linking(const target_model::Target& dep,
                                                const Target_config& dep_config) const
{
  return ignore || allow_includes.allows(dep) || dep_config.ignore ||
         dep_config.interface_allow_includes;
}

bool Target_config::may_link_without_including(const target_model::Target& dep,
                                               const Target_config& dep_config) const
{
  return ignore || allow_links.allows(dep) || dep_config.ignore ||
         dep_config.interface_allow_links;
}

const Target_config& Config::get_target_config(const target_model::Target& target) const
{
  if (const auto it = targets.find(target); it != targets.end())
  {
    return it->second;
  }

  static const Target_config default_target_config;
  return default_target_config;
}
} // namespace lwyi
