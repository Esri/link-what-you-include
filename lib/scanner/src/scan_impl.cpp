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
std::filesystem::path to_normal_path(const std::string& path)
{
  return std::filesystem::path(path).lexically_normal().generic_string();
}

enum class Context : uint8_t
{
  external_file,
  private_file,
  interface_file
};

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
  }
  std::unreachable();
}

class PPRecorder : public clang::PPCallbacks
{
  const clang::Preprocessor& preprocessor_;
  Include_data& include_data_;
  const target_model::Target_data& target_data_;

  Source_line last_include_loc_;
  std::vector<Source_line> include_chain_;
  std::filesystem::path current_source_file_;
  Context context_{Context::external_file};
  bool in_system_ = false;

  std::map<std::filesystem::path, Include_set> header_include_sets_;

  void try_add_dependency_(const std::filesystem::path& source,
                           Context context,
                           const std::filesystem::path& include,
                           Context include_context)
  {
    if (include_context == Context::external_file)
    {
      if (context == Context::interface_file)
      {
        assert(!last_include_loc_.source.empty());
        if (include_data_.include_sets.interface_includes
              .emplace(include, std::vector<Source_line>{last_include_loc_})
              .second)
        {
          message::debug("Interface dependency added: {}", include.string());
        }
      }

      if (auto it = header_include_sets_.find(source); it != header_include_sets_.end())
      {
        if (it->second.emplace(include, std::vector<Source_line>{last_include_loc_}).second)
        {
          message::debug("File dependency added to {}: {}", source.string(), include.string());
        }
      }
    }
  }

  void try_propagate_dependencies_(const Source_line& include_location,
                                   const std::filesystem::path& to_source,
                                   Context to_context,
                                   const std::filesystem::path& from_source,
                                   Context from_context)
  {
    if (from_context == Context::external_file || to_context == Context::external_file)
    {
      return;
    }

    auto from_it = header_include_sets_.find(from_source);
    auto to_it = header_include_sets_.find(to_source);
    if (from_it == header_include_sets_.end() || to_it == header_include_sets_.end())
    {
      return;
    }

    message::debug("Dependency propagation from {} to {}",
                   from_source.string(),
                   to_source.string());

    for (const Include& include : from_it->second)
    {
      std::vector<Source_line> include_chain;
      include_chain.emplace_back(include_location);
      include_chain.insert(include_chain.end(),
                           include.include_chain.begin(),
                           include.include_chain.end());
      to_it->second.emplace(include.path, include_chain);

      if (to_context == Context::interface_file &&
          include_data_.include_sets.interface_includes
            .emplace(include.path, include_chain)
            .second)
      {
        message::debug("Interface dependency added: {}", to_source.string());
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
                        clang::FileID prev_fid,
                        clang::SourceLocation /*loc*/) override
  {
    // ignore predefines file
    if ((reason == LexedFileChangeReason::EnterFile &&
         fid == preprocessor_.getPredefinesFileID()) ||
        (reason == LexedFileChangeReason::ExitFile &&
         prev_fid == preprocessor_.getPredefinesFileID()))
    {
      return;
    }

    // current file
    const auto previous_source_file = current_source_file_;
    if (auto file_entry_ref = preprocessor_.getSourceManager().getFileEntryRefForID(fid);
        file_entry_ref.has_value())
    {
      current_source_file_ = to_normal_path(file_entry_ref->getNameAsRequested().str());
    }
    else
    {
      assert(!"We expect to always get a valid file entry ref");
      context_ = Context::external_file;
      return;
    }

    // context
    const auto previous_context = context_;
    const bool previous_in_system = in_system_;
    in_system_ = clang::SrcMgr::isSystem(file_type);
    const bool in_main = (fid == preprocessor_.getSourceManager().getMainFileID());
    if (!in_main && target_model::is_interface_header(target_data_, current_source_file_))
    {
      context_ = Context::interface_file;
    }
    else if (target_model::is_private_source(target_data_, current_source_file_))
    {
      context_ = Context::private_file;
    }
    else
    {
      context_ = Context::external_file;
    }

    if (!previous_in_system || !in_system_)
    {
      message::debug("{} {} {} ({})",
                     reason == LexedFileChangeReason::EnterFile ? "Enter" : "Reenter",
                     to_string(context_),
                     current_source_file_.string(),
                     fid.getHashValue());
    }

    if (reason == LexedFileChangeReason::EnterFile)
    {
      if (!last_include_loc_.source.empty())
      {
        include_chain_.emplace_back(last_include_loc_);
      }
      if (context_ != Context::external_file)
      {
        header_include_sets_.try_emplace(current_source_file_);
      }
      try_add_dependency_(previous_source_file, previous_context, current_source_file_, context_);
    }
    else
    {
      assert(!include_chain_.empty());
      try_propagate_dependencies_(include_chain_.back(),
                                  current_source_file_,
                                  context_,
                                  previous_source_file,
                                  previous_context);
      include_chain_.pop_back();
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
    assert(current_source_file_ == to_normal_path(presumed_loc.getFilename()));
    last_include_loc_ = Source_line{current_source_file_, presumed_loc.getLine()};

    if (!in_system_)
    {
      message::debug("Include {}{}{} at {}:{}",
                     is_angled ? '<' : '"',
                     spelled_filename.str(),
                     is_angled ? '>' : '"',
                     last_include_loc_.source.string(),
                     last_include_loc_.line);
    }
  }

  void FileSkipped(const clang::FileEntryRef& file,
                   const clang::Token& /*filename_tok*/,
                   clang::SrcMgr::CharacteristicKind /*file_type*/) override
  {
    if (context_ == Context::external_file)
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

    try_add_dependency_(current_source_file_, context_, skipped_filename, skipped_context);
    try_propagate_dependencies_(last_include_loc_,
                                current_source_file_,
                                context_,
                                skipped_filename,
                                skipped_context);
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
        message::debug("Private dependency propagation from {}", main_file.string());
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
