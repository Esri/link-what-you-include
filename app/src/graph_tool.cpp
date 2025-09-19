// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/graph_tool.hpp>

#include <lwyi/strongly_connected_dependencies.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>
#include <util/arg_parser.hpp>

#include <fmt/base.h>
#include <fmt/format.h>
#include <tl/expected.hpp>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr std::string_view usage_string = R"(Usage:
  {0} [options]

Possible options:
  -h, --help                Print this help message.
  -o, --output FILE         Path to the output graphviz dot file to create. An
                            additional file for each strongly connected
                            component will also be created based on this name.)";

struct Options
{
  bool help{false};
  std::string_view output_filename;
};

constexpr auto parser = util::arg_parser<Options>()
                          .arg("-h", "--help", &Options::help)
                          .arg("-o", "--output", &Options::output_filename);

auto usage(std::string_view name) -> std::string
{
  return fmt::format(usage_string, name);
}

auto open_file(const std::filesystem::path& path, const char* mode)
  -> std::unique_ptr<FILE, int (*)(FILE*)>
{
  return {fopen(path.string().c_str(), mode), fclose};
}
} // namespace

auto graph_tool(const target_model::Target_model& target_model,
                const std::vector<target_model::Target>& selected_targets,
                const std::vector<std::string_view>& args) -> int
{
  assert(!args.empty() && args.front() == "graph");

  auto result = parser.parse(args.begin() + 1, args.end());

  if (!result.has_value())
  {
    fmt::print("{}\n{}\n", result.error(), usage("graph"));
    return 1;
  }

  const auto& options = result.value();

  if (options.help)
  {
    fmt::print("{}\n", usage("graph"));
    return 1;
  }

  if (options.output_filename.empty())
  {
    fmt::print("An output file is required.\n{}\n", usage("graph"));
    return 1;
  }

  const auto output_path = std::filesystem::path(options.output_filename);
  const auto path = output_path.parent_path();
  const auto stem = output_path.stem();
  const auto extension = output_path.extension();

  const target_model::Target_model pruned_target_model = [&]()
  {
    if (selected_targets.empty())
    {
      return target_model;
    }

    return target_model.create_pruned(selected_targets);
  }();

  const auto components = lwyi::compute_strongly_connected_dependencies(pruned_target_model);

  auto map_to_component = [&components](const target_model::Target& target) -> std::string
  {
    for (size_t i = 0; i < components.size(); ++i)
    {
      const auto& comp = components[i];
      if (0 < comp.count(target))
      {
        return std::to_string(i);
      }
    }

    return target.name;
  };

  std::set<std::pair<std::string, std::string>> edges;
  pruned_target_model.for_each_target(
    [&](const target_model::Target& target, const target_model::Target_data& target_data)
    {
      const auto component = map_to_component(target);
      for (const auto& dep : target_data.dependencies)
      {
        edges.insert({component, map_to_component(dep)});
      }
    });

  // full graph with non-trivial strongly connected components clumped into a single node
  {
    const auto graph_path = path / (stem.string() + extension.string());
    auto file = open_file(graph_path.string().c_str(), "w");
    if (!file)
    {
      fmt::print("Failed to open file {}\n", graph_path.string());
      return 1;
    }

    fmt::print(file.get(), "digraph dependencies {{\n");

    // nodes for the clusters
    for (size_t i = 0; i < components.size(); ++i)
    {
      const auto& comp = components[i];
      fmt::print(file.get(), "  {} [shape=box label=\"", i);
      bool first = true;
      for (const auto& target : comp)
      {
        if (first)
        {
          first = false;
          fmt::print(file.get(), "{}", target.name);
        }
        else
        {
          fmt::print(file.get(), "\\n{}", target.name);
        }
      }
      fmt::print(file.get(), "\"]\n");
    }

    for (const auto& edge : edges)
    {
      fmt::print(file.get(), "  {} -> {};\n", edge.first, edge.second);
    }
    fmt::print(file.get(), "}}\n");
  }

  // individual graphs for each strongly connected component
  for (size_t i = 0; i < components.size(); ++i)
  {
    const auto& component = components[i];
    const auto component_path =
      path / (stem.string() + "_scc_" + std::to_string(i) + extension.string());
    auto file = open_file(component_path.string().c_str(), "w");
    if (!file)
    {
      fmt::print("Failed to open file {}\n", component_path.string());
      return 1;
    }

    fmt::print(file.get(), "digraph {} {{\n", i);
    for (const auto& target : component)
    {
      if (auto target_data = pruned_target_model.get_target_data(target))
      {
        for (const auto& dep : target_data->get().dependencies)
        {
          if (0 < component.count(dep))
          {
            fmt::print(file.get(), "  {} -> {};\n", target.name, dep.name);
          }
        }
      }
    }
    fmt::print(file.get(), "}}\n");
  }

  return 0;
}
