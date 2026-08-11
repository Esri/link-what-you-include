// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <target_model/target_model.hpp>

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <util/utils.hpp>

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <flat_map>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace target_model
{
Target_model::Target_model(std::map<Target, Target_data> target_to_target_data)
: target_to_target_data_(std::sorted_unique, // NOLINT(misc-include-cleaner) false error
                         std::make_move_iterator(target_to_target_data.begin()),
                         std::make_move_iterator(target_to_target_data.end()))
{
  std::vector<std::filesystem::path> headers;
  std::vector<size_t> indices;
  for (size_t i = 0, iend = target_to_target_data_.keys().size(); i < iend; ++i)
  {
    const Target_data& target_data = target_to_target_data_.values()[i];
    for (const auto& header : target_data.interface_headers)
    {
      headers.push_back(header);
      indices.push_back(i);
    }
    for (const auto& directory : target_data.interface_include_directories)
    {
      directory_to_index_.emplace_back(directory, i);
    }
  }
  header_to_index_ = std::flat_map<std::filesystem::path, size_t>(std::move(headers),
                                                                  std::move(indices));
}

std::string Target_model::validate() const
{
  // check directory_to_target
  // - a directory of one target, cannot be a subdirectory of another.

  for (const auto& [directory, index] : directory_to_index_)
  {
    const auto& target = target_to_target_data_.keys()[index];

    for (const auto& [other_directory, other_index] : directory_to_index_)
    {
      const auto& other_target = target_to_target_data_.keys()[other_index];
      if (target == other_target)
      {
        continue;
      }

      if (util::is_in_directory(directory, other_directory))
      {
        const auto& target_data = target_to_target_data_.values()[index];
        const auto& other_target_data = target_to_target_data_.values()[other_index];

        if (target_data.interface_include_prefixes.empty())
        {
          return std::format(
            "{} and {} have a conflicting include directory ({}) and {} does not have an include prefix to disambiguate.\n",
            target.name,
            other_target.name,
            directory.string(),
            target.name);
        }

        for (const auto& prefix : target_data.interface_include_prefixes)
        {
          if (auto it = other_target_data.interface_include_prefixes.find(prefix);
              it != other_target_data.interface_include_prefixes.end())
          {
            return std::format(
              "{} and {} have a conflicting include directory and share {} as an include prefix.\n",
              target.name,
              other_target.name,
              prefix);
          }
        }
      }
    }
  }

  return {};
}

std::optional<std::reference_wrapper<const Target_data>> Target_model::get_target_data(
  const Target& target) const
{
  if (auto it = target_to_target_data_.find(target); it != target_to_target_data_.end())
  {
    return std::ref(it->second);
  }

  return {};
}

std::optional<Target> Target_model::map_header_to_target(const std::filesystem::path& header) const
{
  if (auto it = header_to_index_.find(header); it != std::end(header_to_index_))
  {
    return {target_to_target_data_.keys()[it->second]};
  }

  for (const auto& [directory, index] : directory_to_index_)
  {
    const auto& target = target_to_target_data_.keys()[index];
    const auto& target_data = target_to_target_data_.values()[index];

    if (target_data.interface_include_prefixes.empty())
    {
      if (util::is_in_directory(directory, header))
      {
        return target;
      }
    }
    else
    {
      for (const auto& prefix : target_data.interface_include_prefixes)
      {
        const auto prefixed_dir = std::filesystem::path{directory} /
                                  std::filesystem::path{prefix};
        if (util::is_in_directory(prefixed_dir, header))
        {
          return target;
        }
      }
    }
  }

  return {};
}

void Target_model::set_interface_include_prefixes(const Target& target,
                                                  const std::set<std::string>& prefixes)
{
  if (auto it = target_to_target_data_.find(target); it != target_to_target_data_.end())
  {
    it->second.interface_include_prefixes = {prefixes.begin(), prefixes.end()};
  }
}

void Target_model::for_each_target(
  const std::function<void(const Target&, const Target_data&)>& visitor) const
{
  for (const auto& [target, data] : target_to_target_data_)
  {
    visitor(target, data);
  }
}

Target_model Target_model::create_pruned(const std::vector<Target>& targets) const
{
  std::map<Target, Target_data> pruned_target_to_target_data;

  std::vector<Target> stack = targets;
  while (!stack.empty())
  {
    auto target = stack.back();
    stack.pop_back();

    if (0 < pruned_target_to_target_data.count(target))
    {
      continue;
    }

    if (auto target_data = get_target_data(target))
    {
      pruned_target_to_target_data.emplace(target, *target_data);
      for (const auto& dep : target_data->get().dependencies)
      {
        stack.push_back(dep);
      }
    }
  }

  return Target_model{std::move(pruned_target_to_target_data)};
}

} // namespace target_model
