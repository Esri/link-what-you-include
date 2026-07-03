// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tidy/config.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace tidy
{
std::expected<Config, std::string> load_config(std::filesystem::path config_path);
}
