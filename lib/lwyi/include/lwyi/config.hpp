// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <target_model/target.hpp>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string_view>

namespace lwyi
{
struct Target_config
{
  bool skip_validation{false};
  std::set<std::string> interface_include_prefixes;
};

struct Config
{
  std::map<target_model::Target, Target_config> targets;

  [[nodiscard]] bool skip_validation(std::string_view target) const
  {
    if (auto it = targets.find(target_model::Target{std::string(target)}); it != targets.end())
    {
      return it->second.skip_validation;
    }

    return false;
  }

  [[nodiscard]] std::optional<std::reference_wrapper<const std::set<std::string>>> interface_include_prefixes(
    std::string_view target) const
  {
    if (auto it = targets.find(target_model::Target{std::string(target)}); it != targets.end())
    {
      return std::cref(it->second.interface_include_prefixes);
    }

    return std::nullopt;
  }
};
} // namespace lwyi
