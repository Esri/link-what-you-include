// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <lwyi/config.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace lwyi
{
std::expected<Config, std::string> load_config(const std::filesystem::path& config_path);
} // namespace lwyi
