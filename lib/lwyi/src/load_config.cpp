// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/load_config.hpp>

#include <json/json_utils.hpp>
#include <lwyi/config.hpp>
#include <src/load_config_impl.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <string>

namespace lwyi
{
std::expected<Config, std::string> load_config(const std::filesystem::path& config_path)
{
  auto raw_config = json::load_file(config_path);
  if (!raw_config.has_value())
  {
    return std::unexpected(raw_config.error());
  }

  return load_config_impl(*raw_config).transform_error(
    [&](const std::string& error)
    {
      return std::format(
        "Failed to parse config file {}: {}", config_path.string(), error);
    });
}
} // namespace lwyi
