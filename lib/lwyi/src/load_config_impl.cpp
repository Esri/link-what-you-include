// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/load_config_impl.hpp>

#include <lwyi/config.hpp>
#include <target_model/target.hpp>

#include <simdjson.h>

#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace lwyi
{
std::expected<Config, std::string> load_config_impl(const simdjson::padded_string& raw_config)
{
  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc;
  if (auto error = parser.iterate(raw_config).get(doc))
  {
    return std::unexpected(std::format("root document: {}", simdjson::error_message(error)));
  }

  Config config;

  simdjson::ondemand::object root;
  if (auto error = doc.get_object().get(root))
  {
    return std::unexpected(std::format("root object: {}", simdjson::error_message(error)));
  }

  simdjson::ondemand::value targets_value;
  if (auto error = root.find_field_unordered("targets").get(targets_value))
  {
    if (error == simdjson::NO_SUCH_FIELD)
    {
      return config;
    }
    return std::unexpected(std::format("field 'targets': {}", simdjson::error_message(error)));
  }

  simdjson::ondemand::object targets;
  if (auto error = targets_value.get_object().get(targets))
  {
    return std::unexpected(std::format("field 'targets': {}", simdjson::error_message(error)));
  }

  for (auto field : targets)
  {
    std::string_view target_name;
    if (auto error = field.unescaped_key().get(target_name))
    {
      return std::unexpected(std::format("target name: {}", simdjson::error_message(error)));
    }

    simdjson::ondemand::object target_object;
    if (auto error = field.value().get_object().get(target_object))
    {
      return std::unexpected(
        std::format("target '{}': {}", target_name, simdjson::error_message(error)));
    }

    Target_config target_config;

    simdjson::ondemand::value skip_validation_value;
    if (!target_object.find_field_unordered("skip_validation").get(skip_validation_value))
    {
      bool skip_validation = false;
      if (auto error = skip_validation_value.get_bool().get(skip_validation))
      {
        return std::unexpected(std::format(
          "target '{}'.skip_validation: {}", target_name, simdjson::error_message(error)));
      }
      target_config.skip_validation = skip_validation;
    }

    simdjson::ondemand::value prefixes_value;
    if (!target_object.find_field_unordered("interface_include_prefixes").get(prefixes_value))
    {
      simdjson::ondemand::array prefixes;
      if (auto error = prefixes_value.get_array().get(prefixes))
      {
        return std::unexpected(std::format("target '{}'.interface_include_prefixes: {}",
                                           target_name,
                                           simdjson::error_message(error)));
      }
      for (auto prefix_value : prefixes)
      {
        std::string_view prefix;
        if (auto error = prefix_value.get(prefix))
        {
          return std::unexpected(std::format("target '{}'.interface_include_prefixes[]: {}",
                                             target_name,
                                             simdjson::error_message(error)));
        }
        target_config.interface_include_prefixes.insert(std::string(prefix));
      }
    }

    config.targets.emplace(target_model::Target{std::string(target_name)}, std::move(target_config));
  }

  return config;
}
} // namespace lwyi
