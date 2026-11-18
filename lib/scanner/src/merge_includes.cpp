// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/merge_includes.hpp>

#include <scanner/scan.hpp>
#include <src/include_sets.hpp>

#include <expected>
#include <string>
#include <vector>

namespace scanner
{
std::expected<Intransitive_includes, std::string> merge_includes(
  std::vector<std::expected<Include_sets, std::string>> include_sets_array)
{
  Include_sets include_sets;
  for (auto& eincdata : include_sets_array)
  {
    if (!eincdata.has_value())
    {
      // TODO: collect all errors
      return std::unexpected(eincdata.error());
    }

    include_sets.includes.merge(eincdata->includes);
    include_sets.interface_includes.merge(eincdata->interface_includes);
  }

  Intransitive_includes output;
  output.interface_includes.insert(output.interface_includes.begin(),
                                   include_sets.interface_includes.begin(),
                                   include_sets.interface_includes.end());
  output.includes.insert(output.includes.begin(),
                         include_sets.includes.begin(),
                         include_sets.includes.end());

  return output;
}
} // namespace scanner
