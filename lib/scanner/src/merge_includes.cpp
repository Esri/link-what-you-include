// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/merge_includes.hpp>

#include <scanner/scan.hpp>
#include <src/scan_impl.hpp>

#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace scanner
{
std::expected<Intransitive_includes, std::string> merge_includes(
  std::vector<std::expected<Include_data, std::string>> include_data_array)
{
  Include_set interface_includes;
  Include_set includes;
  for (auto& eincdata : include_data_array)
  {
    if (!eincdata.has_value())
    {
      // TODO: collect all errors
      return std::unexpected(eincdata.error());
    }

    includes.merge(eincdata->includes);
    for (auto& includes : eincdata->interface_header_includes)
    {
      interface_includes.merge(includes.second);
    }
  }

  Intransitive_includes output;
  output.interface_includes.insert(output.interface_includes.begin(),
                                   interface_includes.begin(),
                                   interface_includes.end());
  output.includes.insert(output.includes.begin(), includes.begin(), includes.end());

  return output;
}
} // namespace scanner
