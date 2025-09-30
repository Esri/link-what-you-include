// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>

namespace scanner
{
auto executable_path() -> std::filesystem::path;
} // namespace scanner
