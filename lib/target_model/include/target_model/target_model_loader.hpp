// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace target_model
{
class Target_model;

class Target_model_loader
{
public:
  static std::unique_ptr<Target_model_loader> create();

  virtual ~Target_model_loader() = default;

  virtual std::expected<void, std::string> load_json(const std::filesystem::path& path) = 0;

  virtual Target_model make_target_model() = 0;
};

} // namespace target_model
