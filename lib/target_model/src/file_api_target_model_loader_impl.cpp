// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/file_api_target_model_loader_impl.hpp>

#include <json/json_utils.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>
#include <target_model/target_model_loader.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
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
// ---------------------------------------------------------------------------
// The parts of the CMake File API codemodel-v2 reply that lwyi reads. Each
// struct mirrors a JSON object. Fields not listed are ignored.
// ---------------------------------------------------------------------------

constexpr uint64_t no_index = std::numeric_limits<uint64_t>::max();

// Fields that only newer codemodel versions provide.
enum class Codemodel_feature : uint8_t
{
  file_set_indexes,
  interface_includes
};

struct Codemodel_version
{
  uint64_t major{0};
  uint64_t minor{0};

  constexpr auto operator<=>(const Codemodel_version&) const = default;

  [[nodiscard]] constexpr bool supports(Codemodel_feature feature) const
  {
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    switch (feature)
    {
      case Codemodel_feature::file_set_indexes:
        return *this >= Codemodel_version{2, 11};
      case Codemodel_feature::interface_includes:
        return *this >= Codemodel_version{2, 12};
    }
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    return false;
  }
};

constexpr Codemodel_version FILE_API_VERSION_REQUIRED{2, 10};

struct Paths
{
  std::string source;
};

struct Target_reference
{
  std::string id;
  std::string name;
  std::string json_file;
};

struct Configuration
{
  std::vector<Target_reference> targets;
  std::vector<Target_reference> abstract_targets;
};

struct Codemodel
{
  Codemodel_version version;
  Paths paths;
  std::vector<Configuration> configurations;
};

struct File_set
{
  std::string type;
  std::string visibility;
};

// Codemodel 2.11 replaced a source's single `fileSetIndex` with
// `fileSetIndexes`; both are read, and file_set_indexes_of() picks by version.
struct Source
{
  std::string path;
  uint64_t file_set_index{no_index};
  std::vector<uint64_t> file_set_indexes;
};

// Codemodel 2.12+.
struct Interface_include
{
  std::string path;
};

struct Dependency
{
  std::string id;
  uint64_t backtrace{no_index};
};

struct Backtrace_node
{
  uint64_t file{no_index};
  uint64_t line{0};
  uint64_t parent{no_index};
};

struct Backtrace_graph
{
  std::vector<std::string> files;
  std::vector<Backtrace_node> nodes;
};

struct Target_reply
{
  std::string id;
  std::string name;
  std::string type;
  bool imported{false};
  std::vector<File_set> file_sets;
  std::vector<Source> sources;
  std::vector<Source> interface_sources;
  std::vector<Interface_include> interface_includes;
  Backtrace_graph backtrace_graph;
  std::vector<Dependency> compile_dependencies;
  std::vector<Dependency> interface_compile_dependencies;
};

auto codemodel_schema()
{
  using json::optional;
  using json::partial_object;
  using json::required;

  const auto target_reference = partial_object<Target_reference>(
    required("id", &Target_reference::id),
    required("name", &Target_reference::name),
    required("jsonFile", &Target_reference::json_file));

  return partial_object<Codemodel>(
    required("version",
             &Codemodel::version,
             partial_object<Codemodel_version>(required("major", &Codemodel_version::major),
                                               required("minor", &Codemodel_version::minor))),
    required("paths", &Codemodel::paths, partial_object<Paths>(required("source", &Paths::source))),
    required("configurations",
             &Codemodel::configurations,
             json::array(partial_object<Configuration>(
               required("targets", &Configuration::targets, json::array(target_reference)),
               optional("abstractTargets",
                        &Configuration::abstract_targets,
                        json::array(target_reference))))));
}

auto target_schema()
{
  using json::optional;
  using json::partial_object;
  using json::required;

  const auto source = partial_object<Source>(
    required("path", &Source::path),
    optional("fileSetIndex", &Source::file_set_index),
    optional("fileSetIndexes", &Source::file_set_indexes, json::array(json::value<uint64_t>())));
  const auto dependency = partial_object<Dependency>(required("id", &Dependency::id),
                                                     optional("backtrace", &Dependency::backtrace));

  return partial_object<Target_reply>(
    required("id", &Target_reply::id),
    required("name", &Target_reply::name),
    required("type", &Target_reply::type),
    optional("imported", &Target_reply::imported),
    optional("fileSets",
             &Target_reply::file_sets,
             json::array(partial_object<File_set>(required("type", &File_set::type),
                                                  required("visibility", &File_set::visibility)))),
    optional("sources", &Target_reply::sources, json::array(source)),
    optional("interfaceSources", &Target_reply::interface_sources, json::array(source)),
    optional("interfaceIncludes",
             &Target_reply::interface_includes,
             json::array(partial_object<Interface_include>(
               required("path", &Interface_include::path)))),
    optional("backtraceGraph",
             &Target_reply::backtrace_graph,
             partial_object<Backtrace_graph>(
               required("files", &Backtrace_graph::files, json::array(json::value<std::string>())),
               required("nodes",
                        &Backtrace_graph::nodes,
                        json::array(partial_object<Backtrace_node>(
                          optional("file", &Backtrace_node::file),
                          optional("line", &Backtrace_node::line),
                          optional("parent", &Backtrace_node::parent)))))),
    optional("compileDependencies", &Target_reply::compile_dependencies, json::array(dependency)),
    optional("interfaceCompileDependencies",
             &Target_reply::interface_compile_dependencies,
             json::array(dependency)));
}

// ---------------------------------------------------------------------------
// Turning replies into Target_data

bool is_linkable_target_type(std::string_view type)
{
  return type == "EXECUTABLE" || type == "STATIC_LIBRARY" || type == "SHARED_LIBRARY" ||
         type == "OBJECT_LIBRARY" || type == "MODULE_LIBRARY";
}

// The location of the innermost backtrace frame that names a file, e.g. the
// target_link_libraries() call that added a dependency.
std::optional<Source_location> backtrace_location(const Backtrace_graph& graph, uint64_t index)
{
  // Bounded by the node count so that a malformed, cyclic graph terminates.
  for (size_t steps = 0; index < graph.nodes.size() && steps < graph.nodes.size(); ++steps)
  {
    const auto& node = graph.nodes[index];
    if (node.file != no_index || node.parent == no_index)
    {
      if (node.file >= graph.files.size())
      {
        return std::nullopt;
      }
      return Source_location{std::filesystem::path(graph.files[node.file]).lexically_normal(),
                             static_cast<size_t>(node.line)};
    }
    index = node.parent;
  }
  return std::nullopt;
}

class Target_data_builder
{
public:
  Target_data_builder(Codemodel_version version,
                      std::filesystem::path source_root,
                      std::map<std::string, std::string> id_to_name)
  : version_(version),
    source_root_(std::move(source_root)),
    id_to_name_(std::move(id_to_name))
  {
  }

  // Paths in replies may be relative to the source directory.
  [[nodiscard]] std::filesystem::path absolute(std::string_view path) const
  {
    const std::filesystem::path result(path);
    return (result.is_relative() ? source_root_ / result : result).lexically_normal();
  }

  [[nodiscard]] Target_data build(const Target_reply& reply) const
  {
    Target_data data;
    data.imported = reply.imported;

    std::set<uint64_t> interface_header_sets;
    for (uint64_t index = 0; index < reply.file_sets.size(); ++index)
    {
      const auto& file_set = reply.file_sets[index];
      if (file_set.type == "HEADERS" &&
          (file_set.visibility == "PUBLIC" || file_set.visibility == "INTERFACE"))
      {
        interface_header_sets.insert(index);
      }
    }

    for (const auto& source : reply.sources)
    {
      auto path = absolute(source.path);
      if (std::ranges::any_of(file_set_indexes_of(source),
                              [&](uint64_t index) { return interface_header_sets.contains(index); }))
      {
        data.interface_headers.insert(path);
      }
      data.sources.insert(std::move(path));
    }
    for (const auto& source : reply.interface_sources)
    {
      data.interface_headers.insert(absolute(source.path));
    }
    if (version_.supports(Codemodel_feature::interface_includes))
    {
      for (const auto& include : reply.interface_includes)
      {
        data.interface_include_directories.insert(absolute(include.path));
      }
    }

    // Interface dependencies first, so that their locations take precedence.
    add_dependencies(reply, reply.interface_compile_dependencies, data.interface_dependencies, data);
    add_dependencies(reply, reply.compile_dependencies, data.dependencies, data);
    return data;
  }

private:
  // The file sets a source belongs to, from whichever field this codemodel
  // version provides.
  [[nodiscard]] std::vector<uint64_t> file_set_indexes_of(const Source& source) const
  {
    if (version_.supports(Codemodel_feature::file_set_indexes))
    {
      return source.file_set_indexes;
    }
    if (source.file_set_index != no_index)
    {
      return {source.file_set_index};
    }
    return {};
  }

  void add_dependencies(const Target_reply& reply,
                        const std::vector<Dependency>& dependencies,
                        std::unordered_set<Target>& destination,
                        Target_data& data) const
  {
    for (const auto& dependency : dependencies)
    {
      const auto name = id_to_name_.find(dependency.id);
      if (name == id_to_name_.end())
      {
        continue;
      }
      const Target target{name->second};
      destination.insert(target);
      if (auto location = backtrace_location(reply.backtrace_graph, dependency.backtrace))
      {
        data.dependency_locations.emplace(target, std::move(*location));
      }
    }
  }

  Codemodel_version version_;
  std::filesystem::path source_root_;
  // Dependencies refer to targets by id; lwyi names them by target name.
  std::map<std::string, std::string> id_to_name_;
};

std::expected<std::filesystem::path, std::string> find_codemodel(
  const std::filesystem::path& reply_dir)
{
  if (!std::filesystem::is_directory(reply_dir))
  {
    return std::unexpected(std::format(
      "error: failed to locate CMake File API reply directory {}", reply_dir.string()));
  }
  for (const auto& entry : std::filesystem::directory_iterator(reply_dir))
  {
    if (entry.is_regular_file() && entry.path().filename().string().starts_with("codemodel-v2-"))
    {
      return entry.path();
    }
  }
  return std::unexpected("error: could not locate codemodel reply");
}

std::expected<std::map<Target, Target_data>, std::string> load_file_api_targets(
  const std::filesystem::path& build_dir)
{
  const auto reply_dir = build_dir / ".cmake" / "api" / "v1" / "reply";
  auto codemodel_path = find_codemodel(reply_dir);
  if (!codemodel_path)
  {
    return std::unexpected(codemodel_path.error());
  }
  auto codemodel = json::read_file(*codemodel_path, codemodel_schema());
  if (!codemodel)
  {
    return std::unexpected(codemodel.error());
  }

  const auto& version = codemodel->version;
  if (version < FILE_API_VERSION_REQUIRED)
  {
    return std::unexpected(std::format(
      "error: lwyi requires CMake file API codemodel version {}.{} or newer; found {}.{}",
      FILE_API_VERSION_REQUIRED.major,
      FILE_API_VERSION_REQUIRED.minor,
      version.major,
      version.minor));
  }
  if (codemodel->configurations.empty())
  {
    return std::unexpected("error: no configurations in codemodel reply");
  }

  // Only the first configuration is used.
  const auto& configuration = codemodel->configurations.front();
  std::vector<Target_reference> references = configuration.targets;
  references.insert(references.end(),
                    configuration.abstract_targets.begin(),
                    configuration.abstract_targets.end());

  std::map<std::string, std::string> id_to_name;
  for (const auto& reference : references)
  {
    id_to_name[reference.id] = reference.name;
  }

  const Target_data_builder builder(
    version, std::filesystem::path(codemodel->paths.source).lexically_normal(), std::move(id_to_name));
  const auto schema = target_schema();
  std::map<Target, Target_data> targets;
  std::map<std::string, std::vector<std::filesystem::path>> verify_sources_by_base_target;
  constexpr std::string_view verify_suffix = "_verify_interface_header_sets";

  for (const auto& reference : references)
  {
    auto reply = json::read_file(reply_dir / reference.json_file, schema);
    if (!reply)
    {
      return std::unexpected(reply.error());
    }

    // CMAKE_VERIFY_INTERFACE_HEADER_SETS targets compile a base target's
    // headers; their sources are recorded on the base target.
    if (reply->name.size() > verify_suffix.size() && reply->name.ends_with(verify_suffix))
    {
      auto& verify_sources =
        verify_sources_by_base_target[reply->name.substr(0, reply->name.size() - verify_suffix.size())];
      for (const auto& source : reply->sources)
      {
        verify_sources.push_back(builder.absolute(source.path));
      }
      continue;
    }

    if (!is_linkable_target_type(reply->type))
    {
      continue;
    }
    auto data = builder.build(*reply);
    if (data.imported && data.interface_headers.empty() &&
        data.interface_include_directories.empty())
    {
      continue;
    }
    if (!targets.emplace(Target{reply->name}, std::move(data)).second)
    {
      return std::unexpected(std::format("Target {} is repeated", reply->name));
    }
  }

  for (const auto& [base_target_name, verify_sources] : verify_sources_by_base_target)
  {
    if (auto it = targets.find(Target{base_target_name}); it != targets.end())
    {
      it->second.verify_interface_header_sets_sources.insert(verify_sources.begin(),
                                                             verify_sources.end());
    }
  }

  return targets;
}
} // namespace

std::expected<void, std::string> File_api_target_model_loader_impl::load_directory(
  const std::filesystem::path& path)
{
  auto targets = load_file_api_targets(path);
  if (!targets)
  {
    return std::unexpected(targets.error());
  }
  target_to_target_data_ = std::move(*targets);
  return {};
}

Target_model File_api_target_model_loader_impl::make_target_model(
  std::map<target_model::Target, std::set<std::string>> target_prefixes)
{
  for (auto& [target, prefixes] : target_prefixes)
  {
    if (auto it = target_to_target_data_.find(target); it != target_to_target_data_.end())
    {
      it->second.interface_include_prefixes.insert(prefixes.begin(), prefixes.end());
    }
  }
  return Target_model{std::exchange(target_to_target_data_, {})};
}

std::unique_ptr<Target_model_loader> Target_model_loader::create()
{
  return std::make_unique<File_api_target_model_loader_impl>();
}
} // namespace target_model
