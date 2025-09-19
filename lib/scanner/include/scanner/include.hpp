// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace scanner
{
struct Source_line
{
  std::filesystem::path source;
  uint32_t line{0};
};

struct Include
{
  std::filesystem::path path;
  std::vector<Source_line> include_chain;
};
} // namespace scanner
