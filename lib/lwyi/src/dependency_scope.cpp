// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/dependency_scope.hpp>

#include <type_traits>

namespace lwyi
{
using Underlying = std::underlying_type_t<Dependency_scope>;

bool operator!(Dependency_scope v)
{
  return v == Dependency_scope::none;
}

Dependency_scope& operator|=(Dependency_scope& lhs, Dependency_scope rhs)

{
  lhs = static_cast<Dependency_scope>(static_cast<Underlying>(lhs) |
                                      static_cast<Underlying>(rhs));
  return lhs;
}

Dependency_scope operator&(Dependency_scope lhs, Dependency_scope rhs)
{
  return static_cast<Dependency_scope>(static_cast<Underlying>(lhs) &
                                       static_cast<Underlying>(rhs));
}
} // namespace lwyi
