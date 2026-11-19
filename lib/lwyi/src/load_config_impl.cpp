// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/load_config_impl.hpp>

#include <json/json_utils.hpp>
#include <lwyi/config.hpp>
#include <target_model/target.hpp>

#include <array>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace lwyi
{
namespace
{
// disallow `ignore` true with any other allow rules
std::expected<void, std::string> check_ignore(const Target_config& config)
{
  if (!config.ignore)
  {
    return {};
  }
  const std::array<std::pair<std::string_view, bool>, 4> allow_fields{{
    {"allow_includes", config.allow_includes != Allowance{}},
    {"allow_links", config.allow_links != Allowance{}},
    {"interface_allow_includes", config.interface_allow_includes},
    {"interface_allow_links", config.interface_allow_links},
  }};
  for (const auto& [name, is_set] : allow_fields)
  {
    if (is_set)
    {
      return std::unexpected(std::format("{} cannot be set when ignore is true", name));
    }
  }
  return {};
}
} // namespace

std::expected<Config, std::string> load_config_impl(std::string_view raw_config)
{
  const auto allowance = json::variant_as<Allowance>(
    json::value<bool>(), json::set(json::string_as<target_model::Target>()));

  const auto target_config_schema = json::validate(
    json::object<Target_config>(
      json::optional("ignore", &Target_config::ignore),
      json::optional("interface_include_prefixes",
                     &Target_config::interface_include_prefixes,
                     json::set(json::value<std::string>())),
      json::optional("allow_includes", &Target_config::allow_includes, allowance),
      json::optional("allow_links", &Target_config::allow_links, allowance),
      json::optional("interface_allow_includes", &Target_config::interface_allow_includes),
      json::optional("interface_allow_links", &Target_config::interface_allow_links)),
    check_ignore);

  const auto config_schema = json::object<Config>(
    json::optional("pedantic", &Config::pedantic),
    json::optional(
      "targets", &Config::targets, json::flat_map<target_model::Target>(target_config_schema)));

  return json::read_root(raw_config, config_schema);
}
} // namespace lwyi
