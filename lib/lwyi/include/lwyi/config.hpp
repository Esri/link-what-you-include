// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <target_model/target.hpp>

#include <flat_map>
#include <set>
#include <string>

namespace lwyi
{
// A set of allowed targets for linking or including, or all
class Allowance
{
public:
  Allowance() = default;
  explicit Allowance(bool all);
  explicit Allowance(std::set<target_model::Target> targets);

  [[nodiscard]] bool allows(const target_model::Target& target) const;
  [[nodiscard]] bool allows_all() const;

  bool operator==(const Allowance&) const = default;

private:
  bool all_{false};
  std::set<target_model::Target> targets_;
};

struct Target_config
{
  bool ignore{false};
  std::set<std::string> interface_include_prefixes{};
  Allowance allow_includes{};
  Allowance allow_links{};
  bool interface_allow_includes{false};
  bool interface_allow_links{false};

  // Whether this target needs no validation at all.
  [[nodiscard]] bool skips_validation() const;

  // Whether this target may include `dep` without linking it.
  [[nodiscard]] bool may_include_without_linking(const target_model::Target& dep,
                                                 const Target_config& dep_config) const;

  // Whether this target may link `dep` without including it.
  [[nodiscard]] bool may_link_without_including(const target_model::Target& dep,
                                                const Target_config& dep_config) const;
};

// The whole config file; each member is the field of the same name.
struct Config
{
  bool pedantic{false};
  std::flat_map<target_model::Target, Target_config> targets{};

  // The target's config, or a default one if the file does not mention it.
  [[nodiscard]] const Target_config& get_target_config(const target_model::Target& target) const;
};
} // namespace lwyi
