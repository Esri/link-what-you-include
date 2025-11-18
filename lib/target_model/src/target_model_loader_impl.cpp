// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/target_model_loader_impl.hpp>

#include <src/file_loader.hpp>
#include <src/real_file_loader.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>
#include <target_model/target_model_loader.hpp>

#include <fmt/format.h>
#include <simdjson.h>
#include <tl/expected.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace target_model
{
namespace
{
constexpr std::array<std::pair<std::string_view, size_t>, 7> table{
  {{"interface_headers", 0},
   {"interface_include_directories", 1},
   {"interface_include_prefixes", 2},
   {"interface_dependencies", 3},
   {"dependencies", 4},
   {"sources", 5},
   {"verify_interface_header_sets_sources", 6}}};

constexpr auto lookup(std::string_view name) -> size_t
{
  for (const auto& [n, v] : table)
  {
    if (name == n)
    {
      return v;
    }
  }

  return table.size();
}

using Raw_data = std::array<std::vector<std::string_view>, table.size()>;

auto parse_target_object_(simdjson::ondemand::object& target_object)
  -> tl::expected<Raw_data, std::string>
{
  Raw_data raw_data{};

  for (auto key_value : target_object)
  {
    std::string_view key;
    if (auto error = key_value.unescaped_key().get(key))
    {
      return tl::unexpected(simdjson::error_message(error));
    }

    simdjson::ondemand::array array;
    if (auto error = key_value.value().get_array().get(array))
    {
      return tl::unexpected(simdjson::error_message(error));
    }

    if (const auto i = lookup(key); i != table.size())
    {
      auto& data_array = raw_data.at(i);
      for (std::string_view e : array)
      {
        data_array.push_back(e);
      }
    }
    else
    {
      return tl::unexpected(fmt::format("Unknown target array name {}", key));
    }
  }

  return raw_data;
}

auto parse_(simdjson::ondemand::parser& parser,
            simdjson::ondemand::document& doc,
            const char* data,
            size_t size,
            size_t size_with_padding)
  -> tl::expected<std::vector<std::pair<Target, Raw_data>>, std::string>
{
  if (auto error = parser.iterate(data, size, size_with_padding).get(doc))
  {
    return tl::unexpected(simdjson::error_message(error));
  }

  simdjson::ondemand::object root_object;
  if (auto error = doc.get_object().get(root_object))
  {
    return tl::unexpected(simdjson::error_message(error));
  }

  std::vector<std::pair<Target, Raw_data>> target_to_raw_data;

  for (auto key_value : root_object)
  {
    std::string_view key;
    if (auto error = key_value.unescaped_key().get(key))
    {
      return tl::unexpected(simdjson::error_message(error));
    }

    target_model::Target target{std::string(key)};

    simdjson::ondemand::object target_object;
    if (auto error = key_value.value().get_object().get(target_object))
    {
      return tl::unexpected(simdjson::error_message(error));
    }

    auto raw_data = parse_target_object_(target_object);
    if (!raw_data.has_value())
    {
      return tl::unexpected(raw_data.error());
    }

    std::pair<Target, Raw_data> pair{std::move(target), std::move(raw_data.value())};
    target_to_raw_data.emplace_back(std::move(pair));
  }

  return target_to_raw_data;
}

auto location_(const simdjson::ondemand::document& doc, const char* data, size_t data_size)
  -> std::string
{
  const char* location = nullptr;
  if (doc.current_location().get(location) || location < data ||
      std::next(data, static_cast<ptrdiff_t>(data_size)) <= location)
  {
    return "line ?, column ?";
  }

  const auto line_start =
    std::find(std::reverse_iterator(location), std::reverse_iterator(data), '\n');
  const size_t line_number = 2UL + std::count(data, &*line_start, '\n');
  const size_t line_offset = location - &*line_start;

  return fmt::format("line {}, column {}", line_number, line_offset);
}

} // namespace

auto Target_model_loader::create() -> std::unique_ptr<Target_model_loader>
{
  return std::make_unique<Target_model_loader_impl>(std::make_unique<Real_file_loader>());
}

Target_model_loader_impl::Target_model_loader_impl(std::unique_ptr<File_loader> file_loader)
: file_loader_(std::move(file_loader))
{
}

auto Target_model_loader_impl::load_json(const std::filesystem::path& path)
  -> tl::expected<void, std::string>
{
  if (!file_loader_->load(path))
  {
    return tl::unexpected(fmt::format("error: failed to load {}", path.string()));
  }

  simdjson::ondemand::document doc;

  auto target_to_raw_data = parse_(parser_,
                                   doc,
                                   file_loader_->data(),
                                   file_loader_->size(),
                                   file_loader_->size_with_padding());
  if (!target_to_raw_data.has_value())
  {
    return tl::unexpected(
      fmt::format("error parsing {}: {}: {}\n",
                  path.string(),
                  location_(doc, file_loader_->data(), file_loader_->size()),
                  target_to_raw_data.error()));
  }

  for (const auto& [target, raw_data] : target_to_raw_data.value())
  {
    Target_data target_data;

    auto& interface_include_directories = raw_data[lookup("interface_include_directories")];
    auto& interface_include_prefixes = raw_data[lookup("interface_include_prefixes")];
    auto& interface_headers = raw_data[lookup("interface_headers")];
    auto& interface_dependencies = raw_data[lookup("interface_dependencies")];
    auto& dependencies = raw_data[lookup("dependencies")];
    auto& sources = raw_data[lookup("sources")];
    auto& verify_interface_header_sets_sources =
      raw_data[lookup("verify_interface_header_sets_sources")];

    target_data.interface_include_directories.insert(interface_include_directories.begin(),
                                                     interface_include_directories.end());
    for (const auto& interface_include_prefix : interface_include_prefixes)
    {
      target_data.interface_include_prefixes.insert(std::string(interface_include_prefix));
    }
    target_data.interface_headers.insert(interface_headers.begin(), interface_headers.end());
    for (const auto& interface_dependency : interface_dependencies)
    {
      target_data.interface_dependencies.insert(Target{std::string(interface_dependency)});
    }
    for (const auto& dependency : dependencies)
    {
      target_data.dependencies.insert(Target{std::string(dependency)});
    }

    target_data.sources.insert(sources.begin(), sources.end());
    target_data.verify_interface_header_sets_sources.insert(
      verify_interface_header_sets_sources.begin(),
      verify_interface_header_sets_sources.end());
    target_to_target_data_.emplace_back(target, target_data);
  }

  return {};
}

auto Target_model_loader_impl::make_target_model() -> Target_model
{
  return Target_model{std::exchange(target_to_target_data_, {})};
}
} // namespace target_model
