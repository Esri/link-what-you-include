// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scanner/include.hpp>
#include <scanner/scan.hpp>

#include <expected>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace llvm
{
template <typename T>
class IntrusiveRefCntPtr;
}

namespace llvm::vfs
{
class FileSystem;
}

namespace clang::tooling::dependencies
{
class DependencyScanningFilesystemSharedCache;
}

namespace target_model
{
struct Target_data;
}

namespace scanner
{
struct Compile_command
{
  std::filesystem::path cwd;
  std::filesystem::path source;
  std::vector<std::string> command;
};

// Here we define a custom set for Includes that ignores the include chain that introduces the dependency. Therefore,
// when a header is included from multiple source files, only one of the include chains is retained.
// TODO: Consider providing an option to store all include chains for display to the user.
struct Include_cmp
{
  bool operator()(const Include& lhs, const Include& rhs) const
  {
    return lhs.path < rhs.path;
  }
};
using Include_set = std::set<Include, Include_cmp>;

struct Include_data
{
  Include_set includes;
  std::map<std::filesystem::path, Include_set> interface_header_includes;
};

std::expected<Include_data, std::string> scan_impl(
  const llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>& file_system,
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache& dep_cache,
  const target_model::Target_data& target_data,
  const Compile_command& compile_command);
} // namespace scanner
