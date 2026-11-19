// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/load_config_impl.hpp>

#include <json/json_utils.hpp>
#include <target_model/target.hpp>
#include <tidy/config.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace tidy
{
std::expected<Config, std::string> load_config_impl(std::string_view raw_config)
{
  constexpr auto targets = json::set(json::string_as<target_model::Target>());
  const auto config_schema = json::object<Config>(
    json::required("forbidden_dependencies",
                   &Config::forbidden_dependencies,
                   json::map<target_model::Target>(targets)),
    json::required("allowed_clusters", &Config::allowed_clusters, json::array(targets)));
  return json::read_root(raw_config, config_schema);
}
} // namespace tidy
