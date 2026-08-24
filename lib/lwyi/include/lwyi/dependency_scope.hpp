// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace lwyi
{
enum class Dependency_scope : uint8_t
{
  none = 0x00,
  private_scope = 0x01,
  interface_scope = 0x10,
  public_scope = 0x11
};

bool operator!(Dependency_scope v);
Dependency_scope& operator|=(Dependency_scope& lhs, Dependency_scope rhs);
Dependency_scope operator&(Dependency_scope lhs, Dependency_scope rhs);
Dependency_scope operator~(Dependency_scope s);
} // namespace lwyi
