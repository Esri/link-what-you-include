// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/scan_impl.hpp>

#include <message/message.hpp>
#include <scanner/include.hpp>
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
#include <memory>
#include <optional>
#include <string>
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

class PPRecorder : public clang::PPCallbacks
{
  const clang::Preprocessor& preprocessor_;
  Include_data& include_data_;
  const target_model::Target_data& target_data_;

  clang::FileID initial_fid_;
  Source_line last_include_loc_;
  std::vector<Source_line> include_chain_;
  std::filesystem::path current_source_file_;
  Include_set* current_include_set_{nullptr};

  enum class Context : uint8_t
  {
    arbitrary_file,
    source_file,
    interface_header
  };
  Context context_{Context::arbitrary_file};

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
                        clang::SrcMgr::CharacteristicKind /*file_type*/,
                        clang::FileID prev_fid,
                        clang::SourceLocation /*loc*/) override
  {
    if ((reason == LexedFileChangeReason::EnterFile &&
         fid == preprocessor_.getPredefinesFileID()) ||
        (reason == LexedFileChangeReason::ExitFile &&
         prev_fid == preprocessor_.getPredefinesFileID()))
    {
      context_ = Context::arbitrary_file;
      return;
    }

    assert(fid.isValid());

    if (initial_fid_.isInvalid())
    {
      initial_fid_ = fid;
    }

    current_source_file_ = to_normal_path(
      preprocessor_.getSourceManager().getSLocEntry(fid).getFile().getName().str());

    message::debug("# {} {}",
                   (reason == LexedFileChangeReason::ExitFile ? "Reenter" : "Enter"),
                   current_source_file_.string());

    const auto previous_context = context_;
    const auto previous_include_set = current_include_set_;

    if (fid != initial_fid_ &&
        target_model::is_interface_header(target_data_, current_source_file_))
    {
      message::debug("Context interface header");
      context_ = Context::interface_header;
      current_include_set_ = &include_data_.interface_header_includes[current_source_file_];
    }
    else if (target_model::is_private_source(target_data_, current_source_file_))
    {
      message::debug("Context source");
      context_ = Context::source_file;
      current_include_set_ = &include_data_.includes;
    }
    else
    {
      message::debug("Context arbitrary file");
      context_ = Context::arbitrary_file;
      current_include_set_ = nullptr;
    }

    if (reason == LexedFileChangeReason::EnterFile)
    {
      if (!last_include_loc_.source.empty())
      {
        message::debug("Push include chain {}", last_include_loc_.source.string());
        include_chain_.emplace_back(last_include_loc_);
      }

      if (previous_include_set && context_ == Context::arbitrary_file)
      {
        message::debug("Dependency added to previous context: {} ({})",
                       current_source_file_.string(),
                       static_cast<const void*>(previous_include_set));
        previous_include_set->emplace(Include{current_source_file_, include_chain_});
      }
    }
    else
    {
      if (!include_chain_.empty())
      {
        message::debug("Pop include chain {}", include_chain_.back().source.string());
        include_chain_.pop_back();
      }

      if (previous_context == Context::interface_header && context_ != Context::arbitrary_file)
      {
        message::debug("Dependency propagation");
        // propagate includes
        for (const auto& e : *previous_include_set)
        {
          message::debug("Dependency added to current context: {} ({})",
                         e.path.string(),
                         static_cast<const void*>(current_include_set_));
          current_include_set_->insert(e);
        }
      }
    }
  }

  void InclusionDirective(clang::SourceLocation include_loc,
                          const clang::Token& /*token*/,
                          clang::StringRef /*spelled_filename*/,
                          bool /*is_angled*/,
                          clang::CharSourceRange /*filename_range*/,
                          clang::OptionalFileEntryRef /*file*/,
                          clang::StringRef /*searchPath*/,
                          clang::StringRef /*relativePath*/,
                          const clang::Module* /*imported*/,
                          clang::SrcMgr::CharacteristicKind /*fileType*/) override
  {
    auto presumed_loc = preprocessor_.getSourceManager().getPresumedLoc(include_loc);
    assert(presumed_loc.isValid());
    assert(current_source_file_ == to_normal_path(presumed_loc.getFilename()));
    last_include_loc_ = Source_line{current_source_file_, presumed_loc.getLine()};
  }

  void FileSkipped(const clang::FileEntryRef& file,
                   const clang::Token& /*filename_tok*/,
                   clang::SrcMgr::CharacteristicKind /*file_type*/) override
  {
    const auto& fileEntry = file.getFileEntry();
    const auto filename = to_normal_path(fileEntry.tryGetRealPathName().str());

    message::debug("file skipped: {}", filename.string());

    if (context_ == Context::arbitrary_file)
    {
      return;
    }

    if (target_model::is_interface_header(target_data_, filename) ||
        target_model::is_private_source(target_data_, filename))
    {
      if (auto it = include_data_.interface_header_includes.find(filename);
          it != include_data_.interface_header_includes.end())
      {
        message::debug("Dependency propagation");
        // propagate includes
        for (const auto& e : it->second)
        {
          message::debug("Dependency added: {} ({})",
                         e.path.string(),
                         static_cast<const void*>(current_include_set_));
          current_include_set_->insert(e);
        }
      }
    }
    else
    {
      message::debug("Dependency added: {} ({})",
                     filename.string(),
                     static_cast<const void*>(current_include_set_));
      auto include_chain = include_chain_;
      if (!last_include_loc_.source.empty())
      {
        include_chain.emplace_back(last_include_loc_);
      }
      current_include_set_->emplace(Include{filename, std::move(include_chain)});
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
            dep_fs->getOrCreateFileSystemEntry(file.getName()))
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
