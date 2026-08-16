// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/file_api_target_model_loader_impl.hpp>

#include <src/file_loader.hpp>
#include <src/real_file_loader.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>
#include <target_model/target_model_loader.hpp>

#include <simdjson.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace target_model
{
namespace
{

constexpr int FILE_API_MAJOR_VERSION_REQUIRED = 2;
constexpr int FILE_API_MINOR_VERSION_REQUIRED = 10;

struct Target_info
{
  std::string id;
  std::string name;
  std::string type;
  bool imported{false};
  std::unordered_set<std::filesystem::path> sources;
  std::unordered_set<std::filesystem::path> public_headers;
  std::vector<std::filesystem::path> include_dirs;
  std::unordered_set<std::string> dependencies;
  std::unordered_set<std::string> interface_dependencies;
  std::map<std::string, Source_location> dep_locations;
};

struct Backtrace_node
{
  std::optional<uint64_t> parent;
  std::optional<uint64_t> file;
  size_t line{0U};
};

struct Backtrace_graph
{
  std::vector<std::string> files;
  std::vector<Backtrace_node> nodes;
};

std::expected<Backtrace_graph, std::string> parse_backtrace_graph(
  simdjson::ondemand::object& graph,
  std::string_view object_context)
{
  Backtrace_graph result;

  simdjson::ondemand::array files;
  if (auto error = graph.find_field_unordered("files").get_array().get(files))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  for (auto file : files)
  {
    std::string_view path;
    if (auto error = file.get(path))
    {
      return std::unexpected(std::format("while reading {}.files: {}",
                                         object_context,
                                         simdjson::error_message(error)));
    }
    result.files.emplace_back(path);
  }

  simdjson::ondemand::array nodes;
  if (auto error = graph.find_field_unordered("nodes").get_array().get(nodes))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  for (auto node : nodes)
  {
    simdjson::ondemand::object node_object;
    if (auto error = node.get_object().get(node_object))
    {
      return std::unexpected(std::format("while reading {}.nodes: {}",
                                         object_context,
                                         simdjson::error_message(error)));
    }

    Backtrace_node backtrace_node;
    simdjson::ondemand::value parent_field;
    if (auto error = node_object.find_field_unordered("parent").get(parent_field))
    {
      if (error != simdjson::NO_SUCH_FIELD)
      {
        return std::unexpected(simdjson::error_message(error));
      }
    }
    else
    {
      uint64_t parent = 0U;
      if (auto error = parent_field.get_uint64().get(parent))
      {
        return std::unexpected(simdjson::error_message(error));
      }
      backtrace_node.parent = parent;
    }

    simdjson::ondemand::value file_field;
    if (auto error = node_object.find_field_unordered("file").get(file_field))
    {
      if (error != simdjson::NO_SUCH_FIELD)
      {
        return std::unexpected(simdjson::error_message(error));
      }
    }
    else
    {
      uint64_t file = 0U;
      if (auto error = file_field.get_uint64().get(file))
      {
        return std::unexpected(simdjson::error_message(error));
      }
      backtrace_node.file = file;
    }

    simdjson::ondemand::value line_field;
    if (auto error = node_object.find_field_unordered("line").get(line_field))
    {
      if (error != simdjson::NO_SUCH_FIELD)
      {
        return std::unexpected(simdjson::error_message(error));
      }
    }
    else
    {
      uint64_t line = 0U;
      if (auto error = line_field.get_uint64().get(line))
      {
        return std::unexpected(simdjson::error_message(error));
      }
      backtrace_node.line = static_cast<size_t>(line);
    }

    result.nodes.push_back(backtrace_node);
  }

  return result;
}

bool is_linkable_target_type(std::string_view type)
{
  return type == "EXECUTABLE" || type == "STATIC_LIBRARY" || type == "SHARED_LIBRARY" ||
         type == "OBJECT_LIBRARY" || type == "MODULE_LIBRARY";
}

std::optional<Source_location> backtrace_location(const Backtrace_graph& graph, uint64_t index)
{
  if (index >= graph.nodes.size())
  {
    return std::nullopt;
  }

  auto current_index = index;
  while (true)
  {
    const auto& node = graph.nodes[static_cast<size_t>(current_index)];

    if (!node.parent.has_value() || node.file.has_value())
    {
      if (node.file.has_value() && *node.file < graph.files.size())
      {
        return Source_location{std::filesystem::path(graph.files[*node.file]).lexically_normal(),
                               node.line};
      }
      return std::nullopt;
    }

    current_index = *node.parent;
  }
}

std::expected<void, std::string> read_dependencies(
  simdjson::ondemand::object& root,
  std::string_view field_name,
  std::unordered_set<std::string>& destination,
  std::map<std::string, Source_location>& dep_locations,
  const std::optional<Backtrace_graph>& backtrace_graph)
{
  simdjson::ondemand::value dependencies_field;
  if (auto error = root.find_field_unordered(field_name).get(dependencies_field))
  {
    if (error == simdjson::NO_SUCH_FIELD)
    {
      return {};
    }
    return std::unexpected(simdjson::error_message(error));
  }

  simdjson::ondemand::array dependencies;
  if (auto error = dependencies_field.get_array().get(dependencies))
  {
    return std::unexpected(simdjson::error_message(error));
  }

  for (auto dep_value : dependencies)
  {
    simdjson::ondemand::object dep;
    if (auto error = dep_value.get_object().get(dep))
    {
      return std::unexpected(simdjson::error_message(error));
    }
    std::string_view id;
    if (auto error = dep.find_field_unordered("id").get(id))
    {
      return std::unexpected(simdjson::error_message(error));
    }
    if (id.empty())
    {
      continue;
    }

    const auto dep_id = std::string(id);
    destination.insert(dep_id);

    if (!backtrace_graph.has_value())
    {
      continue;
    }

    simdjson::ondemand::value backtrace_field;
    if (auto error = dep.find_field_unordered("backtrace").get(backtrace_field);
        error != simdjson::NO_SUCH_FIELD)
    {
      if (error)
      {
        return std::unexpected(simdjson::error_message(error));
      }

      uint64_t backtrace = 0U;
      if (auto error = backtrace_field.get_uint64().get(backtrace))
      {
        return std::unexpected(simdjson::error_message(error));
      }
      if (auto location = backtrace_location(*backtrace_graph, backtrace);
          location.has_value())
      {
        dep_locations.try_emplace(dep_id, *location);
      }
    }
  }

  return {};
}

std::expected<void, std::string> read_json_file(File_loader& file_loader,
                                                simdjson::ondemand::parser& parser,
                                                const std::filesystem::path& path,
                                                simdjson::ondemand::document& doc)
{
  if (!file_loader.load(path))
  {
    return std::unexpected(std::format("error: failed to load {}", path.string()));
  }

  if (auto error =
        parser
          .iterate(file_loader.data(), file_loader.size(), file_loader.size_with_padding())
          .get(doc))
  {
    return std::unexpected(
      std::format("error parsing {}: {}", path.string(), simdjson::error_message(error)));
  }

  return {};
}

std::expected<Target_info, std::string> read_target(const std::filesystem::path& reply_dir,
                                                    File_loader& file_loader,
                                                    simdjson::ondemand::parser& parser,
                                                    const std::string& json_name)
{
  simdjson::ondemand::document doc;
  if (auto result = read_json_file(file_loader, parser, reply_dir / json_name, doc);
      !result.has_value())
  {
    return std::unexpected(result.error());
  }

  simdjson::ondemand::object root;
  if (auto error = doc.get_object().get(root))
  {
    return std::unexpected(simdjson::error_message(error));
  }

  Target_info target;
  std::string_view id;
  if (auto error = root.find_field_unordered("id").get(id))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  target.id = std::string(id);

  std::string_view name;
  if (auto error = root.find_field_unordered("name").get(name))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  target.name = std::string(name);

  std::string_view type;
  if (auto error = root.find_field_unordered("type").get(type))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  target.type = std::string(type);

  target.imported = !root.find_field_unordered("imported").error();

  std::set<uint64_t> interface_header_sets;
  std::vector<std::filesystem::path> pending_base_directories;
  {
    simdjson::ondemand::value file_sets_field;
    if (auto error = root.find_field_unordered("fileSets").get(file_sets_field);
        error != simdjson::NO_SUCH_FIELD)
    {
      if (error)
      {
        return std::unexpected(simdjson::error_message(error));
      }

      simdjson::ondemand::array file_sets;
      if (auto error = file_sets_field.get_array().get(file_sets))
      {
        return std::unexpected(simdjson::error_message(error));
      }

      uint64_t index = 0;
      for (auto file_set_value : file_sets)
      {
        simdjson::ondemand::object file_set;
        if (auto error = file_set_value.get_object().get(file_set))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        std::string_view type;
        if (auto error = file_set.find_field_unordered("type").get(type))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        std::string_view visibility;
        if (auto error = file_set.find_field_unordered("visibility").get(visibility))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        if (type == "HEADERS" && (visibility == "PUBLIC" || visibility == "INTERFACE"))
        {
          interface_header_sets.insert(index);

          simdjson::ondemand::value base_directories_field;
          if (auto error =
                file_set.find_field_unordered("baseDirectories").get(base_directories_field);
              error != simdjson::NO_SUCH_FIELD)
          {
            if (error)
            {
              return std::unexpected(simdjson::error_message(error));
            }

            simdjson::ondemand::array base_directories;
            if (auto error = base_directories_field.get_array().get(base_directories))
            {
              return std::unexpected(simdjson::error_message(error));
            }

            for (auto base_directory_value : base_directories)
            {
              std::string_view base_directory;
              if (auto error = base_directory_value.get(base_directory))
              {
                return std::unexpected(simdjson::error_message(error));
              }
              pending_base_directories.push_back(
                std::filesystem::path(base_directory).lexically_normal());
            }
          }
        }
        ++index;
      }
    }
  }

  {
    simdjson::ondemand::value sources_field;
    if (auto error = root.find_field_unordered("sources").get(sources_field);
        error != simdjson::NO_SUCH_FIELD)
    {
      if (error)
      {
        return std::unexpected(simdjson::error_message(error));
      }

      simdjson::ondemand::array sources;
      if (auto error = sources_field.get_array().get(sources))
      {
        return std::unexpected(simdjson::error_message(error));
      }

      for (auto source_value : sources)
      {
        simdjson::ondemand::object source;
        if (auto error = source_value.get_object().get(source))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        std::string_view path;
        if (auto error = source.find_field_unordered("path").get(path))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        target.sources.insert(std::filesystem::path(path).lexically_normal());

        simdjson::ondemand::value file_set_indexes_field;
        if (auto error =
              source.find_field_unordered("fileSetIndexes").get(file_set_indexes_field);
            error != simdjson::NO_SUCH_FIELD)
        {
          if (error)
          {
            return std::unexpected(simdjson::error_message(error));
          }

          simdjson::ondemand::array file_set_indexes;
          if (auto error = file_set_indexes_field.get_array().get(file_set_indexes))
          {
            return std::unexpected(simdjson::error_message(error));
          }

          for (auto file_set_index_value : file_set_indexes)
          {
            uint64_t file_set_index = 0U;
            if (auto error = file_set_index_value.get_uint64().get(file_set_index))
            {
              return std::unexpected(simdjson::error_message(error));
            }
            if (interface_header_sets.contains(file_set_index))
            {
              target.public_headers.insert(std::filesystem::path(path).lexically_normal());
              break;
            }
          }
        }
      }
    }
  }

  {
    simdjson::ondemand::value interface_sources_field;
    if (auto error = root.find_field_unordered("interfaceSources").get(interface_sources_field);
        error != simdjson::NO_SUCH_FIELD)
    {
      if (error)
      {
        return std::unexpected(simdjson::error_message(error));
      }

      simdjson::ondemand::array interface_sources;
      if (auto error = interface_sources_field.get_array().get(interface_sources))
      {
        return std::unexpected(simdjson::error_message(error));
      }

      for (auto source_value : interface_sources)
      {
        simdjson::ondemand::object source;
        if (auto error = source_value.get_object().get(source))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        std::string_view path;
        if (auto error = source.find_field_unordered("path").get(path))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        target.public_headers.insert(std::filesystem::path(path).lexically_normal());
      }
    }
  }

  if (target.public_headers.empty())
  {
    target.include_dirs.insert(target.include_dirs.end(),
                               pending_base_directories.begin(),
                               pending_base_directories.end());
  }

  std::optional<Backtrace_graph> backtrace_graph;
  simdjson::ondemand::value backtrace_graph_field;
  if (auto error = root.find_field_unordered("backtraceGraph").get(backtrace_graph_field);
      error != simdjson::NO_SUCH_FIELD)
  {
    if (error)
    {
      return std::unexpected(simdjson::error_message(error));
    }

    simdjson::ondemand::object backtrace_graph_object;
    if (auto error = backtrace_graph_field.get_object().get(backtrace_graph_object))
    {
      return std::unexpected(simdjson::error_message(error));
    }

    auto parsed_backtrace_graph = parse_backtrace_graph(backtrace_graph_object, json_name);
    if (!parsed_backtrace_graph.has_value())
    {
      return std::unexpected(parsed_backtrace_graph.error());
    }
    backtrace_graph = std::move(parsed_backtrace_graph.value());
  }

  if (auto result = read_dependencies(root,
                                      "interfaceCompileDependencies",
                                      target.interface_dependencies,
                                      target.dep_locations,
                                      backtrace_graph);
      !result.has_value())
  {
    return std::unexpected(result.error());
  }
  if (auto result = read_dependencies(root,
                                      "compileDependencies",
                                      target.dependencies,
                                      target.dep_locations,
                                      backtrace_graph);
      !result.has_value())
  {
    return std::unexpected(result.error());
  }

  return target;
}

std::expected<std::vector<std::pair<Target, Target_data>>, std::string> load_file_api_targets(
  const std::filesystem::path& build_dir,
  File_loader& file_loader,
  simdjson::ondemand::parser& parser)
{
  const auto reply_dir = build_dir / ".cmake" / "api" / "v1" / "reply";
  if (!std::filesystem::is_directory(reply_dir))
  {
    return std::unexpected(
      std::format("error: failed to locate CMake File API reply directory {}",
                  reply_dir.string()));
  }

  std::filesystem::path codemodel_path;
  for (const auto& entry : std::filesystem::directory_iterator(reply_dir))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }

    const auto filename = entry.path().filename().string();
    if (filename.starts_with("codemodel-v2-"))
    {
      codemodel_path = entry.path();
      break;
    }
  }

  if (codemodel_path.empty())
  {
    return std::unexpected("error: could not locate codemodel reply");
  }

  simdjson::ondemand::document codemodel_doc;
  if (auto result = read_json_file(file_loader, parser, codemodel_path, codemodel_doc);
      !result.has_value())
  {
    return std::unexpected(result.error());
  }

  simdjson::ondemand::object codemodel_root;
  if (auto error = codemodel_doc.get_object().get(codemodel_root))
  {
    return std::unexpected(simdjson::error_message(error));
  }

  simdjson::ondemand::object version;
  if (auto error = codemodel_root.find_field_unordered("version").get_object().get(version))
  {
    return std::unexpected(simdjson::error_message(error));
  }

  uint64_t major = 0;
  if (auto error = version.find_field_unordered("major").get_uint64().get(major))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  uint64_t minor = 0;
  if (auto error = version.find_field_unordered("minor").get_uint64().get(minor))
  {
    return std::unexpected(simdjson::error_message(error));
  }

  if (major != FILE_API_MAJOR_VERSION_REQUIRED || minor < FILE_API_MINOR_VERSION_REQUIRED)
  {
    return std::unexpected(std::format(
      "error: lwyi requires CMake file API codemodel version {}.{} or newer; found {}.{}",
      FILE_API_MAJOR_VERSION_REQUIRED,
      FILE_API_MINOR_VERSION_REQUIRED,
      major,
      minor));
  }

  simdjson::ondemand::object paths;
  if (auto error = codemodel_root.find_field_unordered("paths").get_object().get(paths))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  std::string_view source_dir_string;
  if (auto error = paths.find_field_unordered("source").get(source_dir_string))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  const auto source_root = std::filesystem::path(source_dir_string).lexically_normal();

  simdjson::ondemand::array configurations;
  if (auto error =
        codemodel_root.find_field_unordered("configurations").get_array().get(configurations))
  {
    return std::unexpected(simdjson::error_message(error));
  }
  auto configuration_it = configurations.begin();
  if (configuration_it == configurations.end())
  {
    return std::unexpected("error: no configurations in codemodel reply");
  }

  simdjson::ondemand::object configuration;
  if (auto error = (*configuration_it).get_object().get(configuration))
  {
    return std::unexpected(simdjson::error_message(error));
  }

  std::map<std::string, std::string> id_to_name;
  std::map<std::string, std::vector<std::filesystem::path>> verify_sources_by_base_target;
  std::vector<std::pair<Target, Target_data>> target_to_target_data;

  std::vector<std::pair<std::string, std::string>> target_id_to_json;
  auto collect_targets = [&](std::string_view field_name,
                             bool required) -> std::expected<void, std::string>
  {
    simdjson::ondemand::value targets_field;
    if (auto error = configuration.find_field_unordered(field_name).get(targets_field);
        error != simdjson::NO_SUCH_FIELD)
    {
      if (error)
      {
        return std::unexpected(simdjson::error_message(error));
      }

      simdjson::ondemand::array targets;
      if (auto error = targets_field.get_array().get(targets))
      {
        return std::unexpected(simdjson::error_message(error));
      }

      for (auto target_value : targets)
      {
        simdjson::ondemand::object target;
        if (auto error = target_value.get_object().get(target))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        std::string_view id;
        if (auto error = target.find_field_unordered("id").get(id))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        std::string_view name;
        if (auto error = target.find_field_unordered("name").get(name))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        std::string_view json_file;
        if (auto error = target.find_field_unordered("jsonFile").get(json_file))
        {
          return std::unexpected(simdjson::error_message(error));
        }
        id_to_name[std::string(id)] = std::string(name);
        target_id_to_json.emplace_back(std::string(id), std::string(json_file));
      }
    }
    else if (required)
    {
      return std::unexpected(simdjson::error_message(error));
    }

    return {};
  };

  if (auto result = collect_targets("targets", true); !result.has_value())
  {
    return std::unexpected(result.error());
  }
  if (auto result = collect_targets("abstractTargets", false); !result.has_value())
  {
    return std::unexpected(result.error());
  }

  for (const auto& [id, json_file] : target_id_to_json)
  {
    auto target_info = read_target(reply_dir, file_loader, parser, json_file);
    if (!target_info.has_value())
    {
      return std::unexpected(target_info.error());
    }

    constexpr std::string_view verify_suffix = "_verify_interface_header_sets";
    if (target_info->name.size() > verify_suffix.size() &&
        target_info->name.ends_with(verify_suffix))
    {
      const auto base_target_name =
        target_info->name.substr(0, target_info->name.size() - verify_suffix.size());
      std::vector<std::filesystem::path> verify_sources;
      verify_sources.reserve(target_info->sources.size());
      for (const auto& source : target_info->sources)
      {
        auto candidate = source.is_relative() ? source_root / source : source;
        verify_sources.push_back(candidate.lexically_normal());
      }
      verify_sources_by_base_target[std::string(base_target_name)] =
        std::move(verify_sources);
      continue;
    }

    if (!is_linkable_target_type(target_info->type))
    {
      continue;
    }
    if (target_info->imported && target_info->public_headers.empty() &&
        target_info->include_dirs.empty())
    {
      continue;
    }

    Target_data target_data;
    target_data.imported = target_info->imported;
    for (const auto& include_dir : target_info->include_dirs)
    {
      auto candidate = include_dir.is_relative() ? source_root / include_dir : include_dir;
      target_data.interface_include_directories.insert(candidate.lexically_normal());
    }
    for (const auto& header : target_info->public_headers)
    {
      auto candidate = header.is_relative() ? source_root / header : header;
      target_data.interface_headers.insert(candidate.lexically_normal());
    }
    for (const auto& source : target_info->sources)
    {
      auto candidate = source.is_relative() ? source_root / source : source;
      target_data.sources.insert(candidate.lexically_normal());
    }
    for (const auto& dep_id : target_info->dependencies)
    {
      auto it = id_to_name.find(dep_id);
      if (it == id_to_name.end())
      {
        continue;
      }

      const auto dep_target = Target{it->second};
      target_data.dependencies.insert(dep_target);
      if (auto location_it = target_info->dep_locations.find(dep_id);
          location_it != target_info->dep_locations.end())
      {
        target_data.dependency_locations.emplace(dep_target, location_it->second);
      }
    }
    for (const auto& dep_id : target_info->interface_dependencies)
    {
      auto it = id_to_name.find(dep_id);
      if (it == id_to_name.end())
      {
        continue;
      }

      const auto dep_target = Target{it->second};
      target_data.interface_dependencies.insert(dep_target);
      if (auto location_it = target_info->dep_locations.find(dep_id);
          location_it != target_info->dep_locations.end())
      {
        target_data.dependency_locations.emplace(dep_target, location_it->second);
      }
    }

    target_to_target_data.emplace_back(Target{target_info->name}, std::move(target_data));
  }

  for (const auto& [base_target_name, verify_sources] : verify_sources_by_base_target)
  {
    auto it = std::ranges::find_if(target_to_target_data,
                                   [&](const auto& pair)
                                   {
                                     return pair.first.name == base_target_name;
                                   });
    if (it != target_to_target_data.end())
    {
      it->second.verify_interface_header_sets_sources.insert(verify_sources.begin(),
                                                             verify_sources.end());
    }
  }

  return target_to_target_data;
}
} // namespace

File_api_target_model_loader_impl::File_api_target_model_loader_impl(
  std::unique_ptr<File_loader> file_loader)
: file_loader_(std::move(file_loader))
{
}

std::expected<void, std::string> File_api_target_model_loader_impl::load_directory(
  const std::filesystem::path& path)
{
  auto target_to_target_data = load_file_api_targets(path, *file_loader_, parser_);
  if (!target_to_target_data.has_value())
  {
    return std::unexpected(target_to_target_data.error());
  }

  target_to_target_data_ = std::move(target_to_target_data.value());
  return {};
}

Target_model File_api_target_model_loader_impl::make_target_model()
{
  return Target_model{std::exchange(target_to_target_data_, {})};
}

std::unique_ptr<Target_model_loader> Target_model_loader::create()
{
  return std::make_unique<File_api_target_model_loader_impl>(
    std::make_unique<Real_file_loader>());
}
} // namespace target_model
