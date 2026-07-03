// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/strongly_connected_dependencies.hpp>

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace lwyi
{
namespace
{
struct Vertex_data
{
  int index{-1};
  int lowlink{-1};
  bool on_stack{false};
};
} // namespace

std::vector<std::set<target_model::Target>> compute_strongly_connected_dependencies(
  const target_model::Target_model& target_model)
{
  using Component = std::set<target_model::Target>;

  int index = 0;
  std::vector<target_model::Target> stack;
  std::vector<Component> strongly_connected;
  std::map<target_model::Target, Vertex_data> data;

  std::function<void(target_model::Target v)> strong_connect;
  strong_connect = [&](const target_model::Target& v)
  {
    auto& vdata = data[v];
    vdata.index = index;
    vdata.lowlink = index;
    vdata.on_stack = true;

    stack.push_back(v);
    ++index;

    if (auto target_data = target_model.get_target_data(v))
    {
      for (const auto& w : target_data->get().dependencies)
      {
        auto& wdata = data[w];
        if (wdata.index == -1)
        {
          strong_connect(w);
          vdata.lowlink = std::min(vdata.lowlink, wdata.lowlink);
        }
        else if (wdata.on_stack)
        {
          vdata.lowlink = std::min(vdata.lowlink, wdata.index);
        }
      }
    }

    if (vdata.lowlink == vdata.index)
    {
      Component c;
      for (;;)
      {
        auto w = stack.back();
        stack.pop_back();
        auto& wdata = data[w];
        wdata.on_stack = false;
        c.insert(w);
        if (w == v)
        {
          break;
        }
      }
      // only interested in non-trivial components
      if (1 < c.size())
      {
        strongly_connected.push_back(std::move(c));
      }
    }
  };

  target_model.for_each_target(
    [&](const target_model::Target& v, const target_model::Target_data&)
    {
      auto& vdata = data[v];
      if (vdata.index == -1)
      {
        strong_connect(v);
      }
    });

  return strongly_connected;
}
} // namespace lwyi
