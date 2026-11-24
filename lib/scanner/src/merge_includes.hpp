// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scanner/scan.hpp>
#include <src/include_sets.hpp>

#include <expected>
#include <string>
#include <vector>

namespace scanner
{
std::expected<Intransitive_includes, std::string> merge_includes(
  std::vector<std::expected<Include_sets, std::string>> include_sets_array);
} // namespace scanner
