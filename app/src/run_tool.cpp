// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/run_tool.hpp>

#include <message/message.hpp>
#include <src/graph_tool.hpp>
#include <src/tidy_tool.hpp>


#include <cassert>
#include <string_view>
#include <vector>

namespace
{
constexpr std::string_view usage_string = R"(tools:
  list                      Print this help message.
  tidy                      Check that the dependency graph is a DAG.
  graph                     Generate a graphviz dot graph of the dependencies.)";
}

auto run_tool(const target_model::Target_model& target_model,
              const std::vector<target_model::Target>& selected_targets,
              const std::vector<std::string_view>& args) -> int
{
  assert(!args.empty());

  if (args[0] == "list")
  {
    message::print(usage_string);
    return 0;
  }

  if (args[0] == "graph")
  {
    return graph_tool(target_model, selected_targets, args);
  }

  if (args[0] == "tidy")
  {
    return tidy_tool(target_model, selected_targets, args);
  }

  message::error("Unknown tool {}", args[0]);
  return 1;
}
