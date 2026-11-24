// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scanner/include.hpp>

#include <set>

namespace scanner
{
// Here we define a custom set for Includes that ignores the include chain that introduces the dependency. Therefore,
// when a header is included from multiple source files, only one of the include chains is retained.
// TODO: Consider providing an option to store all include chains for display to the user.
struct Include_cmp
{
  bool operator()(const Include& lhs, const Include& rhs) const
  {
    return lhs.path < rhs.path;
  }
};
using Include_set = std::set<Include, Include_cmp>;

struct Include_sets
{
  Include_set interface_includes;
  Include_set includes;

  bool operator==(const Include_sets&) const = default;
};
} // namespace scanner
