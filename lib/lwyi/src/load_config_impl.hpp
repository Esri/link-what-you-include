// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <lwyi/config.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace lwyi
{
std::expected<Config, std::string> load_config_impl(std::string_view raw_config);
} // namespace lwyi
