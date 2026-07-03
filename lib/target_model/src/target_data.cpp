// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <target_model/target_data.hpp>

#include <util/utils.hpp>

#include <filesystem>

namespace target_model
{
bool is_interface_header(const Target_data& target_data, const std::filesystem::path& filename)
{
  if (target_data.interface_headers.contains(filename))
  {
    return true;
  }

  for (const auto& directory : target_data.interface_include_directories)
  {
    if (target_data.interface_include_prefixes.empty())
    {
      if (util::is_in_directory(directory, filename))
      {
        return true;
      }
    }
    else
    {
      for (const auto& prefix : target_data.interface_include_prefixes)
      {
        if (util::is_in_directory(directory / prefix, filename))
        {
          return true;
        }
      }
    }
  }

  return false;
}

bool is_private_source(const Target_data& target_data, const std::filesystem::path& filename)
{
  return target_data.sources.contains(filename) || target_data.headers.contains(filename);
}
} // namespace target_model
