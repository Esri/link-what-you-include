// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <tidy/config.hpp>

#include <simdjson.h>

#include <expected>
#include <string>

namespace tidy
{
std::expected<Config, std::string> load_config_impl(const simdjson::padded_string& raw_config);
}
