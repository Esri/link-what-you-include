// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <scanner/scan.hpp>

#include <src/executable_path.hpp>
#include <src/merge_includes.hpp>
#include <src/scan_impl.hpp>
#include <target_model/target_data.hpp>
#include <util/parallel_transformer.hpp>

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/DependencyScanning/DependencyScanningFilesystem.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <tl/expected.hpp>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#define LWYI_STRING(s) #s
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LWYI_STRINGIFY(s) LWYI_STRING(s)

namespace scanner
{
struct Scanner::Impl
{
  explicit Impl(size_t thread_count)
  : transformer(thread_count)
  {
  }

  util::Parallel_transformer transformer;
};

Scanner::Scanner(size_t thread_count)
: impl_(std::make_unique<Impl>(thread_count))
{
}

Scanner::~Scanner() = default;

auto Scanner::scan(const std::filesystem::path& binary_dir,
                   const target_model::Target_data& target_data)
  -> tl::expected<Intransitive_includes, std::string>
{
  std::string compilation_database_error;
  auto compilation_database = clang::tooling::JSONCompilationDatabase::loadFromFile(
    binary_dir.string() + "/compile_commands.json",
    compilation_database_error,
    clang::tooling::JSONCommandLineSyntax::AutoDetect);
  if (!compilation_database)
  {
    return tl::unexpected(fmt::format("Failed to load compilation database: {}\n",
                                      compilation_database_error));
  }

  std::vector<std::filesystem::path> source_paths;
  source_paths.reserve(target_data.sources.size() +
                       target_data.verify_interface_header_sets_sources.size());
  for (const auto& file : target_data.sources)
  {
    source_paths.emplace_back(file);
  }
  for (const auto& file : target_data.verify_interface_header_sets_sources)
  {
    source_paths.emplace_back(file);
  }

  const auto exe_path = executable_path();
  const auto resource_dir = exe_path.parent_path() / LWYI_STRINGIFY(LWYI_RELATIVE_RESOURCE_DIR);

  clang::tooling::CommandLineArguments arguments;
  arguments.emplace_back(fmt::format("-resource-dir={}", resource_dir.string()));
#if _WIN32
  arguments.emplace_back("-Wno-error");
  arguments.emplace_back("-Wno-unused-command-line-argument");
#endif
  auto args_adjuster =
    getInsertArgumentAdjuster(arguments, clang::tooling::ArgumentInsertPosition::END);
  args_adjuster =
    clang::tooling::combineAdjusters(std::move(args_adjuster),
                                     clang::tooling::getClangStripOutputAdjuster());
  args_adjuster =
    clang::tooling::combineAdjusters(std::move(args_adjuster),
                                     clang::tooling::getClangSyntaxOnlyAdjuster());
  args_adjuster =
    clang::tooling::combineAdjusters(std::move(args_adjuster),
                                     clang::tooling::getClangStripDependencyFileAdjuster());

  size_t processed_file_count = 0;
  std::map<std::string, size_t> skipped_file_types;

  std::vector<Compile_command> compile_commands;
  for (const auto& source_path : source_paths)
  {
    if (!source_path.is_absolute())
    {
      return tl::unexpected(fmt::format("Unexpected relative path in target data: {}\n",
                                        source_path.string()));
    }

    std::vector<clang::tooling::CompileCommand> compile_commands_for_file =
      compilation_database->getCompileCommands(source_path.string());
    if (compile_commands_for_file.empty())
    {
      ++skipped_file_types[source_path.extension().string()];
      continue;
    }
    for (clang::tooling::CompileCommand& compile_command : compile_commands_for_file)
    {
      auto command_line =
        args_adjuster(compile_command.CommandLine, compile_command.Filename);
      assert(!command_line.empty());
      compile_commands.emplace_back(
        Compile_command{compile_command.Directory, source_path, std::move(command_line)});
    }
    ++processed_file_count;
  }

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  std::vector<tl::expected<Include_data, std::string>> include_data_array(
    compile_commands.size());

  impl_->transformer.transform(
    compile_commands.begin(),
    compile_commands.end(),
    include_data_array.begin(),
    [&](const Compile_command& compile_command) -> tl::expected<Include_data, std::string>
    {
      auto file_system = llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>{
        llvm::vfs::createPhysicalFileSystem()};
      return scan_impl(file_system, dep_cache, target_data, compile_command);
    });

  fmt::print("Processed {} source files\n", processed_file_count);
  for (const auto& skipped_file_type : skipped_file_types)
  {
    auto msg = 1 == skipped_file_type.second ? "file" : "files";
    fmt::print("Skipped {} *{} {}\n", skipped_file_type.second, skipped_file_type.first, msg);
  }

  return merge_includes(include_data_array);
}
} // namespace scanner
