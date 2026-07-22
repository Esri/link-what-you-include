// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/load_config.hpp>

#include <src/load_config_impl.hpp>
#include <lwyi/config.hpp>

#include <simdjson.h>

#include <expected>
#include <filesystem>
#include <format>
#include <string>

namespace lwyi
{
std::expected<Config, std::string> load_config(const std::filesystem::path& config_path)
{
  simdjson::padded_string raw_config;
  if (auto error = simdjson::padded_string::load(config_path.string()).get(raw_config))
  {
    return std::unexpected(std::format(
      "Failed to load config file {}: {}", config_path.string(), simdjson::error_message(error)));
  }

  auto config = load_config_impl(raw_config);
  if (!config.has_value())
  {
    return std::unexpected(std::format(
      "Failed to parse config file {}: {}", config_path.string(), config.error()));
  }

  return config;
}
} // namespace lwyi
