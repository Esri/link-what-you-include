// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/load_config.hpp>

#include <lwyi/config.hpp>
#include <target_model/target.hpp>

#include <simdjson.h>

#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace lwyi
{
namespace
{
auto parse_error(const std::filesystem::path& config_path,
                 std::string_view context,
                 simdjson::error_code error) -> std::unexpected<std::string>
{
  return std::unexpected(std::format("Failed to parse config file {} ({}): {}",
                                     config_path.string(),
                                     context,
                                     simdjson::error_message(error)));
}
} // namespace

std::expected<Config, std::string> load_config(const std::filesystem::path& config_path)
{
  simdjson::padded_string raw_config;
  if (auto error = simdjson::padded_string::load(config_path.string()).get(raw_config))
  {
    return std::unexpected(std::format(
      "Failed to load config file {}: {}", config_path.string(), simdjson::error_message(error)));
  }

  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc;
  if (auto error = parser.iterate(raw_config).get(doc))
  {
    return parse_error(config_path, "root document", error);
  }

  Config config;

  simdjson::ondemand::object root;
  if (auto error = doc.get_object().get(root))
  {
    return parse_error(config_path, "root object", error);
  }

  simdjson::ondemand::value targets_value;
  if (auto error = root.find_field_unordered("targets").get(targets_value))
  {
    if (error == simdjson::NO_SUCH_FIELD)
    {
      return config;
    }
    return parse_error(config_path, "field 'targets'", error);
  }

  simdjson::ondemand::object targets;
  if (auto error = targets_value.get_object().get(targets))
  {
    return parse_error(config_path, "field 'targets'", error);
  }

  for (auto field : targets)
  {
    std::string_view target_name;
    if (auto error = field.unescaped_key().get(target_name))
    {
      return parse_error(config_path, "target name", error);
    }

    simdjson::ondemand::object target_object;
    if (auto error = field.value().get_object().get(target_object))
    {
      return parse_error(
        config_path, std::format("target '{}'", target_name), error);
    }

    Target_config target_config;

    simdjson::ondemand::value skip_validation_value;
    if (!target_object.find_field_unordered("skip_validation").get(skip_validation_value))
    {
      bool skip_validation = false;
      if (auto error = skip_validation_value.get_bool().get(skip_validation))
      {
        return parse_error(
          config_path, std::format("target '{}'.skip_validation", target_name), error);
      }
      target_config.skip_validation = skip_validation;
    }

    simdjson::ondemand::value prefixes_value;
    if (!target_object.find_field_unordered("interface_include_prefixes").get(prefixes_value))
    {
      simdjson::ondemand::array prefixes;
      if (auto error = prefixes_value.get_array().get(prefixes))
      {
        return parse_error(config_path,
                           std::format("target '{}'.interface_include_prefixes", target_name),
                           error);
      }
      for (auto prefix_value : prefixes)
      {
        std::string_view prefix;
        if (auto error = prefix_value.get(prefix))
        {
          return parse_error(config_path,
                             std::format("target '{}'.interface_include_prefixes[]", target_name),
                             error);
        }
        target_config.interface_include_prefixes.insert(std::string(prefix));
      }
    }

    config.targets.emplace(target_model::Target{std::string(target_name)}, std::move(target_config));
  }

  return config;
}
} // namespace lwyi
