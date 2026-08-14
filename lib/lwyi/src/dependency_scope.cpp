// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/dependency_visibility.hpp>

#include <type_traits>

namespace lwyi
{
using Underlying = std::underlying_type_t<Dependency_visibility>;

bool operator!(Dependency_visibility v)
{
  return v == Dependency_visibility::none;
}

Dependency_visibility& operator|=(Dependency_visibility& lhs, Dependency_visibility rhs)

{
  lhs = static_cast<Dependency_visibility>(static_cast<Underlying>(lhs) |
                                           static_cast<Underlying>(rhs));
  return lhs;
}

Dependency_visibility operator&(Dependency_visibility lhs, Dependency_visibility rhs)
{
  return static_cast<Dependency_visibility>(static_cast<Underlying>(lhs) &
                                            static_cast<Underlying>(rhs));
}
} // namespace lwyi
