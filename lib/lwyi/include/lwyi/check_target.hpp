// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <lwyi/dependency_scope.hpp>
#include <scanner/include.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>

#include <optional>
#include <vector>

namespace scanner
{
struct Intransitive_includes;
}

namespace target_model
{
struct Target_data;
class Target_model;
} // namespace target_model

namespace lwyi
{
struct LWYI_error
{
  target_model::Target target;
  Dependency_scope linked_scope;
  Dependency_scope included_scope;
  std::optional<target_model::Source_location> linked_location;
  std::vector<scanner::Include> sample_includes;
};

std::vector<LWYI_error> check_target(const target_model::Target_model& target_model,
                                     const target_model::Target& target,
                                     const target_model::Target_data& target_data,
                                     const scanner::Intransitive_includes& target_includes);
} // namespace lwyi
