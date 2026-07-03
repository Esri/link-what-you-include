// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <src/file_loader.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace target_model
{
class Real_file_loader : public File_loader
{
public:
  [[nodiscard]] std::expected<void, std::string> load(const std::filesystem::path& path) override;
  [[nodiscard]] const char* data() const override;
  [[nodiscard]] size_t size() const override;
  [[nodiscard]] size_t size_with_padding() const override;

private:
  size_t size_{0U};
  std::vector<char> bytes_;
};
} // namespace target_model
