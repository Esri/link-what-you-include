// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <target_model/target_model.hpp>

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <util/utils.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace target_model
{
namespace
{
struct Less
{
  using Element = std::pair<Target, Target_data>;

  auto operator()(const std::pair<Target, Target_data>& lhs,
                  const std::pair<Target, Target_data>& rhs) -> bool
  {
    return lhs.first < rhs.first;
  }

  auto operator()(const Target& lhs, const std::pair<Target, Target_data>& rhs) -> bool
  {
    return lhs < rhs.first;
  }

  auto operator()(const std::pair<Target, Target_data>& lhs, const Target& rhs) -> bool
  {
    return lhs.first < rhs;
  }

  auto operator()(const std::pair<std::filesystem::path, const Element*>& lhs,
                  const std::pair<std::filesystem::path, const Element*>& rhs) -> bool
  {
    return lhs.first < rhs.first;
  }

  auto operator()(const std::filesystem::path& header,
                  const std::pair<std::filesystem::path, const Element*>& pair) -> bool
  {
    return header < pair.first;
  }

  auto operator()(const std::pair<std::filesystem::path, const Element*>& pair,
                  const std::filesystem::path& header) -> bool
  {
    return pair.first < header;
  }
};

struct Comp
{
  auto operator()(const std::pair<Target, Target_data>& lhs,
                  const std::pair<Target, Target_data>& rhs) -> bool
  {
    return lhs.first == rhs.first;
  }
};
} // namespace

Target_model::Target_model(std::vector<std::pair<Target, Target_data>> target_to_target_data)
: target_to_target_data_(std::move(target_to_target_data))
{
  std::sort(target_to_target_data_.begin(), target_to_target_data_.end(), Less{});

  for (const Element& element : target_to_target_data_)
  {
    const Target_data& target_data = element.second;
    for (const auto& header : target_data.interface_headers)
    {
      header_to_target_[header] = &element;
    }
    for (const auto& directory : target_data.interface_include_directories)
    {
      directory_to_target_.emplace_back(directory, &element);
    }
  }
}

auto Target_model::validate() const -> std::string
{
  // look for duplicate targets
  if (auto it = std::adjacent_find(target_to_target_data_.begin(),
                                   target_to_target_data_.end(),
                                   Comp{});
      it != target_to_target_data_.end())
  {
    return fmt::format("Target {} is repeated.\n", it->first.name);
  }

  // check directory_to_target
  // - a directory of one target, cannot be a subdirectory of another.

  for (const auto& pair : directory_to_target_)
  {
    const auto& directory = pair.first;
    const auto& target = pair.second->first;

    for (const auto& other_pair : directory_to_target_)
    {
      const auto& other_target = other_pair.second->first;
      if (target == other_target)
      {
        continue;
      }
      const auto& other_directory = other_pair.first;
      if (util::is_in_directory(directory, other_directory))
      {
        const auto& target_data = pair.second->second;
        const auto& other_target_data = other_pair.second->second;

        if (target_data.interface_include_prefixes.empty())
        {
          return fmt::format(
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
            return fmt::format(
              "{} and {} have conflicting include directories and share {} as an include prefix.\n",
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

auto Target_model::get_target_data(const Target& target) const
  -> std::optional<std::reference_wrapper<const Target_data>>
{
  if (auto it = std::lower_bound(target_to_target_data_.begin(),
                                 target_to_target_data_.end(),
                                 target,
                                 Less{});
      it != target_to_target_data_.end() && it->first == target)
  {
    return std::ref(it->second);
  }

  return {};
}

auto Target_model::map_header_to_target(const std::filesystem::path& header) const
  -> std::optional<Target>
{
  if (auto it = header_to_target_.find(header);
      it != std::end(header_to_target_) && it->first == header)
  {
    return {it->second->first};
  }

  for (const auto& pair : directory_to_target_)
  {
    const auto& directory = pair.first;
    const auto& target = pair.second->first;
    const auto& target_data = pair.second->second;

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

auto Target_model::for_each_target(
  const std::function<void(const Target&, const Target_data&)>& visitor) const -> void
{
  for (auto& [target, data] : target_to_target_data_)
  {
    visitor(target, data);
  }
}

auto Target_model::create_pruned(const std::vector<Target>& targets) const -> Target_model
{
  std::map<Target, Target_data> pruned_target_to_target_map;

  std::vector<Target> stack = targets;
  while (!stack.empty())
  {
    auto target = stack.back();
    stack.pop_back();

    if (0 < pruned_target_to_target_map.count(target))
    {
      continue;
    }

    if (auto target_data = get_target_data(target))
    {
      pruned_target_to_target_map.emplace(target, *target_data);
      for (const auto& dep : target_data->get().dependencies)
      {
        stack.push_back(dep);
      }
    }
  }

  std::vector<std::pair<Target, Target_data>> pruned_target_to_target_data;
  pruned_target_to_target_data.reserve(pruned_target_to_target_map.size());
  for (auto& [target, target_data] : pruned_target_to_target_map)
  {
    pruned_target_to_target_data.emplace_back(target, target_data);
  }

  return Target_model{pruned_target_to_target_data};
}

} // namespace target_model
