// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <expected>
#include <filesystem>

namespace target_model
{
class File_loader
{
public:
  virtual ~File_loader() = default;
  [[nodiscard]] virtual std::expected<void, std::string> load(
    const std::filesystem::path& path) = 0;
  [[nodiscard]] virtual const char* data() const = 0;
  [[nodiscard]] virtual size_t size() const = 0;
  [[nodiscard]] virtual size_t size_with_padding() const = 0;
};
} // namespace target_model
