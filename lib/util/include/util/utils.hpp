// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>

namespace util
{
bool is_in_directory(const std::filesystem::path& dir, const std::filesystem::path& file);
}
