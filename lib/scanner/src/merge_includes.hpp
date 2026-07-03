// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scanner/scan.hpp>
#include <src/scan_impl.hpp>

#include <expected>
#include <string>
#include <vector>

namespace scanner
{
struct Include_data;

std::expected<Intransitive_includes, std::string> merge_includes(
  std::vector<std::expected<Include_data, std::string>> include_data_array);
} // namespace scanner
