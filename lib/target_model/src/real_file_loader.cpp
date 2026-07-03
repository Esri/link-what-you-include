// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/real_file_loader.hpp>

#include <simdjson.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <string>

namespace target_model
{
std::expected<void, std::string> Real_file_loader::load(const std::filesystem::path& path)

{
  std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
  if (ifs.fail())
  {
    return std::unexpected(std::format("Failed to open {}", path.string()));
  }

  size_ = ifs.tellg();
  ifs.seekg(0, std::ios::beg);
  bytes_.resize(size_ + simdjson::SIMDJSON_PADDING);

  ifs.read(bytes_.data(), static_cast<std::streamsize>(size_));
  if (ifs.fail())
  {
    return std::unexpected(std::format("Failed to read {}", path.string()));
  }

  return {};
}

const char* Real_file_loader::data() const
{
  return bytes_.data();
}

size_t Real_file_loader::size() const
{
  return size_;
}

size_t Real_file_loader::size_with_padding() const
{
  return bytes_.size();
}
} // namespace target_model
