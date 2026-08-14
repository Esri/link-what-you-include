// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/dependency_scope.hpp>

#include <utility>

namespace lwyi
{
bool operator!(Dependency_scope v)
{
  return v == Dependency_scope::none;
}

Dependency_scope& operator|=(Dependency_scope& lhs, Dependency_scope rhs)

{
  lhs = static_cast<Dependency_scope>(std::to_underlying(lhs) | std::to_underlying(rhs));
  return lhs;
}

Dependency_scope operator&(Dependency_scope lhs, Dependency_scope rhs)
{
  return static_cast<Dependency_scope>(std::to_underlying(lhs) & std::to_underlying(rhs));
}
} // namespace lwyi
