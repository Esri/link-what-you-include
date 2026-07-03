#include <src/load_config_impl.hpp>

#include <target_model/target.hpp>
#include <tidy/config.hpp>

#include <simdjson.h>

#include <expected>
#include <format>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tidy
{
std::expected<Config, std::string> load_config_impl(const simdjson::padded_string& raw_config)
{
  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc;
  if (auto error = parser.iterate(raw_config).get(doc))
  {
    return std::unexpected(
      std::format("Error parsing json: {}\n", simdjson::error_message(error)));
  }

  Config config;

  simdjson::ondemand::object forbidden_dependencies;
  if (auto error = doc["forbidden_dependencies"].get_object().get(forbidden_dependencies))
  {
    return std::unexpected(std::format("Error parsing forbidden_dependencies: {}\n",
                                       simdjson::error_message(error)));
  }
  for (auto key_value : forbidden_dependencies)
  {
    std::string_view key;
    if (auto error = key_value.unescaped_key().get(key))
    {
      return std::unexpected(std::format("Error parsing forbidden_dependencies: {}\n",
                                         simdjson::error_message(error)));
    }
    target_model::Target target{std::string(key)};
    std::set<target_model::Target> dependencies;

    simdjson::ondemand::array array;
    if (auto error = key_value.value().get_array().get(array))
    {
      return std::unexpected(std::format("Error parsing forbidden_dependencies: {}\n",
                                         simdjson::error_message(error)));
    }
    for (std::string_view e : array)
    {
      dependencies.insert(target_model::Target{std::string(e)});
    }
    config.forbidden_dependencies[target] = std::move(dependencies);
  }

  simdjson::ondemand::array allowed_clusters_array;
  if (auto error = doc["allowed_clusters"].get_array().get(allowed_clusters_array))
  {
    return std::unexpected(std::format("Error parsing allowed_clusters: {}\n",
                                       simdjson::error_message(error)));
  }

  for (auto targets_array : allowed_clusters_array)
  {
    simdjson::ondemand::array cluster_array;
    if (auto error = targets_array.get_array().get(cluster_array))
    {
      return std::unexpected(std::format("Error parsing allowed_clusters: {}\n",
                                         simdjson::error_message(error)));
    }
    std::set<target_model::Target> cluster;
    for (auto target_string : cluster_array)
    {
      std::string_view target_name;
      if (auto error = target_string.get(target_name))
      {
        return std::unexpected(
          std::format("Error parsing target: {}\n", simdjson::error_message(error)));
      }

      cluster.insert(target_model::Target{std::string(target_name)});
    }
    config.allowed_clusters.push_back(std::move(cluster));
  }

  return config;
}
} // namespace tidy
