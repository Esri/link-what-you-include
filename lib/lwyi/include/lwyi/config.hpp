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

  [[nodiscard]] bool skip_validation(std::string_view target) const;

  [[nodiscard]] std::optional<std::reference_wrapper<const std::set<std::string>>> interface_include_prefixes(
    std::string_view target) const;
};
} // namespace lwyi
