// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace target_model
{
struct Target
{
  std::string name;

  auto operator<=>(const Target&) const = default;
};
} // namespace target_model

template <>
struct std::hash<target_model::Target>
{
  std::size_t operator()(const target_model::Target& target) const noexcept
  {
    return std::hash<std::string>{}(target.name);
  }
};
