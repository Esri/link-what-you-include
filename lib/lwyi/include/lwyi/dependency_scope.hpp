// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace lwyi
{
enum class Dependency_visibility : uint8_t
{
  none = 0x00,
  private_scope = 0x01,
  interface_scope = 0x10,
  public_scope = 0x11
};

bool operator!(Dependency_visibility v);
Dependency_visibility& operator|=(Dependency_visibility& lhs, Dependency_visibility rhs);
Dependency_visibility operator&(Dependency_visibility lhs, Dependency_visibility rhs);
} // namespace lwyi
