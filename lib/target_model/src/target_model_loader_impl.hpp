// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <src/file_loader.hpp>
#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model_loader.hpp>

#include <simdjson.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace target_model
{
class File_loader;
class Target_model;

class Target_model_loader_impl : public Target_model_loader
{
public:
  explicit Target_model_loader_impl(std::unique_ptr<File_loader> file_loader);

  std::expected<void, std::string> load_json(const std::filesystem::path& path) override;

  Target_model make_target_model() override;

private:
  std::unique_ptr<File_loader> file_loader_;
  simdjson::ondemand::parser parser_;
  std::vector<std::pair<Target, Target_data>> target_to_target_data_;
};

} // namespace target_model
