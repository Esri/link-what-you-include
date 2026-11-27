// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/scan_impl.hpp>

#include <message/message.hpp>
#include <scanner/include.hpp>
#include <src/include_sets.hpp>
#include <target_model/target_data.hpp>

#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileEntry.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/LLVM.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <clang/Tooling/DependencyScanning/DependencyScanningFilesystem.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <cassert>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace clang::dependency_directives_scan
{
struct Directive;
} // namespace clang::dependency_directives_scan

namespace scanner
{
namespace
{
std::filesystem::path to_normal_path(const std::string& path)
{
  return std::filesystem::path(path).lexically_normal().generic_string();
}

enum class Context : uint8_t
{
  private_file,
  interface_file,
  external_file,
  external_system_file
};

bool is_external(Context context)
{
  return context == Context::external_file || context == Context::external_system_file;
}

std::string_view to_string(Context context)
{
  switch (context)
  {
    case Context::interface_file:
      return "interface";
    case Context::private_file:
      return "private";
    case Context::external_file:
      return "external";
    case Context::external_system_file:
      return "system";
  }
  std::unreachable();
}

struct Frame
{
  Source_line location;
  Context context{Context::private_file};
};

std::string to_string(const std::filesystem::path& source)
{
  return std::format("{}", source.empty() ? "<built-in>" : source.string());
}

class PPRecorder : public clang::PPCallbacks
{
  const clang::Preprocessor& preprocessor_;
  Include_data& include_data_;
  const target_model::Target_data& target_data_;
  std::stack<Frame> stack_;
  std::map<std::filesystem::path, Include_set> header_include_sets_;

  void try_add_dependency_(const std::filesystem::path& include,
                           Context include_context)
  {
    if (is_external(include_context))
    {
      assert(!stack_.empty());
      const auto& frame = stack_.top();
      const auto location = frame.location;
      const auto context = frame.context;

      if (context == Context::interface_file)
      {
        if (include_data_.include_sets.interface_includes
              .emplace(include, std::vector<Source_line>{frame.location})
              .second)
        {
          message::debug("Interface dependency added: {}", include.string());
        }
      }

      if (auto it = header_include_sets_.find(location.source); it != header_include_sets_.end())
      {
        std::vector<Source_line> include_chain;
        include_chain.emplace_back(frame.location);
        if (it->second.emplace(include, include_chain).second)
        {
          message::debug("File dependency added to {}: {}",
                         to_string(location.source),
                         include.string());
        }
      }
    }
  }

  void try_propagate_dependencies_(const std::filesystem::path& from_source,
                                   Context from_context)
  {
    assert(!stack_.empty());
    const auto& frame = stack_.top();
    const auto to_context = frame.context;
    if (is_external(from_context) || is_external(to_context))
    {
      return;
    }

    const auto to_source = frame.location.source;
    auto from_it = header_include_sets_.find(from_source);
    auto to_it = header_include_sets_.find(to_source);
    if (from_it == header_include_sets_.end() || to_it == header_include_sets_.end())
    {
      return;
    }

    message::debug("Dependency propagation from {} to {}",
                   to_string(from_source),
                   to_string(to_source));

    for (const Include& include : from_it->second)
    {
      // Prepend the include_chain with the current location unless we are at line 0. This only happens when the
      // <built-in> file is entered "from" the main file.
      std::vector<Source_line> include_chain;
      if (const auto include_location = frame.location; include_location.line != 0)
      {
        include_chain.emplace_back(include_location);
      }
      include_chain.insert(include_chain.end(),
                           include.include_chain.begin(),
                           include.include_chain.end());
      to_it->second.emplace(include.path, include_chain);

      if (to_context == Context::interface_file &&
          include_data_.include_sets.interface_includes
            .emplace(include.path, include_chain)
            .second)
      {
        message::debug("Interface dependency added: {}", include.path.string());
      }
    }
  }

public:
  PPRecorder(const clang::Preprocessor& preprocessor,
             Include_data& include_data,
             const target_model::Target_data& target_data)
  : preprocessor_(preprocessor),
    include_data_(include_data),
    target_data_(target_data)
  {
  }

  ~PPRecorder() override = default;
  PPRecorder(const PPRecorder&) = delete;
  PPRecorder(PPRecorder&&) noexcept = delete;
  PPRecorder& operator=(const PPRecorder&) = delete;
  PPRecorder& operator=(PPRecorder&&) noexcept = delete;

  void LexedFileChanged(clang::FileID fid,
                        LexedFileChangeReason reason,
                        clang::SrcMgr::CharacteristicKind file_type,
                        clang::FileID /*prev_fid*/,
                        clang::SourceLocation /*loc*/) override
  {
    if (reason == LexedFileChangeReason::EnterFile)
    {
      const bool in_main = (fid == preprocessor_.getSourceManager().getMainFileID());
      const bool in_system = clang::SrcMgr::isSystem(file_type);
      Context context = Context::private_file;
      std::filesystem::path current_source_file;
      if (auto file_entry_ref = preprocessor_.getSourceManager().getFileEntryRefForID(fid);
          file_entry_ref.has_value())
      {
        current_source_file = to_normal_path(file_entry_ref->getNameAsRequested().str());
        if (!in_main && target_model::is_interface_header(target_data_, current_source_file))
        {
          context = Context::interface_file;
        }
        else if (target_model::is_private_source(target_data_, current_source_file))
        {
          context = Context::private_file;
        }
        else if (in_system)
        {
          context = Context::external_system_file;
        }
        else
        {
          context = Context::external_file;
        }
      }

      message::debug("Enter {} {} ({})",
                     to_string(context),
                     to_string(current_source_file),
                     fid.getHashValue());

      if (!is_external(context))
      {
        header_include_sets_.try_emplace(current_source_file);
      }
      if (!in_main)
      {
        try_add_dependency_(current_source_file, context);
      }
      stack_.emplace(Source_line{current_source_file, 0}, context);
    }
    else
    {
      assert(2 <= stack_.size());
      const auto previous_frame = std::move(stack_.top());
      stack_.pop();
      const auto& frame = stack_.top();
      message::debug("Reenter {} {} ({})",
                     to_string(frame.context),
                     to_string(frame.location.source),
                     fid.getHashValue());
      try_propagate_dependencies_(previous_frame.location.source, previous_frame.context);
    }
  }

  void InclusionDirective(clang::SourceLocation include_loc,
                          const clang::Token& /*token*/,
                          clang::StringRef spelled_filename,
                          bool is_angled,
                          clang::CharSourceRange /*filename_range*/,
                          clang::OptionalFileEntryRef /*file*/,
                          clang::StringRef /*searchPath*/,
                          clang::StringRef /*relativePath*/,
                          const clang::Module* /*imported*/,
                          clang::SrcMgr::CharacteristicKind /*file_type*/) override
  {
    auto presumed_loc = preprocessor_.getSourceManager().getPresumedLoc(include_loc);
    assert(presumed_loc.isValid());
    assert(!stack_.empty());
    auto& current_frame = stack_.top();
    current_frame.location.line = presumed_loc.getLine();
    if (current_frame.context != Context::external_system_file)
    {
      message::debug("Include {}{}{} at {}:{}",
                     is_angled ? '<' : '"',
                     spelled_filename.str(),
                     is_angled ? '>' : '"',
                     to_string(current_frame.location.source),
                     current_frame.location.line);
    }
  }

  void FileSkipped(const clang::FileEntryRef& file,
                   const clang::Token& /*filename_tok*/,
                   clang::SrcMgr::CharacteristicKind file_type) override
  {
    assert(!stack_.empty());
    const auto context = stack_.top().context;
    if (is_external(context))
    {
      return;
    }

    const auto skipped_filename = to_normal_path(file.getNameAsRequested().str());

    message::debug("File skipped: {}", skipped_filename.string());

    Context skipped_context = Context::external_file;
    if (target_model::is_interface_header(target_data_, skipped_filename))
    {
      skipped_context = Context::interface_file;
    }
    else if (target_model::is_private_source(target_data_, skipped_filename))
    {
      skipped_context = Context::private_file;
    }
    else if (clang::SrcMgr::isSystem(file_type))
    {
      skipped_context = Context::external_system_file;
    }

    try_add_dependency_(skipped_filename, skipped_context);
    try_propagate_dependencies_(skipped_filename, skipped_context);
  }

  void EndOfMainFile() override
  {
    const auto main_fid = preprocessor_.getSourceManager().getMainFileID();
    if (auto main_file_entry_ref =
          preprocessor_.getSourceManager().getFileEntryRefForID(main_fid);
        main_file_entry_ref.has_value())
    {
      auto main_file = to_normal_path(main_file_entry_ref->getNameAsRequested().str());
      if (auto it = header_include_sets_.find(main_file); it != header_include_sets_.end())
      {
        message::debug("Private dependency propagation from {}", to_string(main_file));
        include_data_.include_sets.includes.swap(it->second);
      }
    }
  }
};

class Action : public clang::PreprocessOnlyAction
{
  Include_data& include_data_;
  const target_model::Target_data& target_data_;

  llvm::IntrusiveRefCntPtr<clang::tooling::dependencies::DependencyScanningWorkerFilesystem> dep_fs_;

public:
  Action(Include_data& include_data,
         const target_model::Target_data& target_data,
         llvm::IntrusiveRefCntPtr<clang::tooling::dependencies::DependencyScanningWorkerFilesystem> dep_fs)
  : include_data_(include_data),
    target_data_(target_data),
    dep_fs_(std::move(dep_fs))
  {
  }

private:
  void ExecuteAction() override
  {
    auto& compiler_instance = getCompilerInstance();

    compiler_instance.getDiagnosticOpts().IgnoreWarnings = true;
    compiler_instance.getDiagnostics().setIgnoreAllWarnings(true);

    compiler_instance.getPreprocessorOpts().DependencyDirectivesForFile =
      [dep_fs = dep_fs_](clang::FileEntryRef file)
      -> std::optional<llvm::ArrayRef<clang::dependency_directives_scan::Directive>>
    {
      if (llvm::ErrorOr<clang::tooling::dependencies::EntryRef> entry =
            dep_fs->getOrCreateFileSystemEntry(file.getNameAsRequested()))
      {
        return entry->getDirectiveTokens();
      }
      return std::nullopt;
    };

    auto& preprocessor = compiler_instance.getPreprocessor();
    preprocessor.addPPCallbacks(
      std::make_unique<PPRecorder>(preprocessor, include_data_, target_data_));

    PreprocessOnlyAction::ExecuteAction();
  }
};

class Action_factory : public clang::tooling::FrontendActionFactory
{
public:
  Action_factory(Include_data& include_data,
                 const target_model::Target_data& target_data,
                 llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> file_system,
                 clang::tooling::dependencies::DependencyScanningFilesystemSharedCache& dep_cache)
  : include_data_(include_data),
    target_data_(target_data),
    file_system_(std::move(file_system)),
    dep_cache_(dep_cache)
  {
  }

  ~Action_factory() override = default;
  Action_factory(const Action_factory&) = delete;
  Action_factory(Action_factory&&) noexcept = delete;
  Action_factory& operator=(const Action_factory&) = delete;
  Action_factory& operator=(Action_factory&&) noexcept = delete;

  std::unique_ptr<clang::FrontendAction> create() override
  {
    auto dep_fs =
      llvm::IntrusiveRefCntPtr<clang::tooling::dependencies::DependencyScanningWorkerFilesystem>{
        new clang::tooling::dependencies::DependencyScanningWorkerFilesystem(dep_cache_,
                                                                             file_system_)};
    return std::make_unique<Action>(include_data_, target_data_, std::move(dep_fs));
  }

private:
  Include_data& include_data_;
  const target_model::Target_data& target_data_;
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> file_system_;
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache& dep_cache_;
};
} // namespace

std::expected<Include_data, std::string> scan_impl(
  const llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>& file_system,
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache& dep_cache,
  const target_model::Target_data& target_data,
  const Compile_command& compile_command)
{
  if (file_system->setCurrentWorkingDirectory(compile_command.cwd.string()))
  {
    return std::unexpected(std::format("Cannot chdir into {}", compile_command.cwd.string()));
  }

  Include_data include_data;
  Action_factory action_factory(include_data, target_data, file_system, dep_cache);
  auto file_manager = llvm::IntrusiveRefCntPtr<clang::FileManager>{
    new clang::FileManager(clang::FileSystemOptions(), file_system)};
  auto pch_container_ops = std::make_shared<clang::PCHContainerOperations>();

  clang::tooling::ToolInvocation invocation(compile_command.command,
                                            &action_factory,
                                            file_manager.get(),
                                            pch_container_ops);
  if (!invocation.run())
  {
    return std::unexpected(
      std::format("Error while processing {}.\n", compile_command.source.string()));
  }

  return include_data;
}
} // namespace scanner
