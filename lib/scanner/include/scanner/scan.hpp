// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scanner/include.hpp>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace target_model
{
struct Target_data;
}

namespace scanner
{
struct Intransitive_includes
{
  std::vector<Include> interface_includes;
  std::vector<Include> includes;
};

class Scanner
{
public:
  explicit Scanner(size_t thread_count);
  ~Scanner();
  Scanner(const Scanner&) = delete;
  Scanner(Scanner&&) = delete;
  Scanner& operator=(const Scanner&) = delete;
  Scanner& operator=(Scanner&&) = delete;

  std::expected<Intransitive_includes, std::string> scan(
    const std::filesystem::path& binary_dir,
    const target_model::Target_data& target_data);

private:
  struct Impl;

  std::unique_ptr<Impl> impl_;
};

} // namespace scanner
