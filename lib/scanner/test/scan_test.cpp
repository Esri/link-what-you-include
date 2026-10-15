// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <scanner/scan.hpp>

#include <message/message.hpp>
#include <scanner/include.hpp>
#include <src/merge_includes.hpp>
#include <src/scan_impl.hpp>
#include <target_model/target_data.hpp>

#include <catch2/catch_test_macros.hpp>
#include <clang/Tooling/DependencyScanning/DependencyScanningFilesystem.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <print>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
void dump(const scanner::Include_set& include_set, std::string_view indent = "")
{
  for (const auto& include : include_set)
  {
    std::print("{}{}\n", indent, include.path.string());
    for (const auto& source_line : include.include_chain)
    {
      std::print("{}  {}:{}\n", indent, source_line.source.string(), source_line.line);
    }
  }
}

[[maybe_unused]] void dump(const scanner::Include_data& include_data)
{
  std::print("includes:\n");
  dump(include_data.includes, "  ");

  std::print("interface_includes:\n");
  dump(include_data.interface_includes, "  ");
}

[[maybe_unused]] void dump(const scanner::Intransitive_includes& ii)
{
  std::print("interface includes:\n");
  for (const auto& interface_include : ii.interface_includes)
  {
    std::print("  {}\n", interface_include.path.string());
    for (const auto& source_line : interface_include.include_chain)
    {
      std::print("    {}:{}\n", source_line.source.string(), source_line.line);
    }
  }

  std::print("includes:\n");
  for (const auto& include : ii.includes)
  {
    std::print("  {}\n", include.path.string());
    for (const auto& source_line : include.include_chain)
    {
      std::print("    {}:{}\n", source_line.source.string(), source_line.line);
    }
  }
}

struct Literal_file
{
  const char* path;
  const char* content;
};

void add_file(llvm::vfs::InMemoryFileSystem& fs, const Literal_file& file)
{
  fs.addFile(file.path, 0, llvm::MemoryBuffer::getMemBuffer(file.content));
}
} // namespace

TEST_CASE("scanner: basic scan test", "[scanner]")
{
  // Given:
  //         private.cpp 
  //           ╱     ╲
  //   interface.hpp  ╲
  //         ╱         ╲
  //      a.hpp       b.hpp
  //
  // Expect:
  //   dependencies:           a.hpp and b.hpp
  //   interface dependencies: a.hpp

  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // 3rdparty
  Literal_file a_hpp{"/a.hpp", ""};
  Literal_file b_hpp{"/b.hpp", ""};

  // interface
  Literal_file interface_hpp{"/interface.hpp", R"(
    #include "a.hpp"
    )"};

  // private
  Literal_file private_cpp{"/private.cpp", R"(
    #include "interface.hpp"
    #include "b.hpp"
    )"};

  add_file(*fs, a_hpp);
  add_file(*fs, b_hpp);
  add_file(*fs, interface_hpp);
  add_file(*fs, private_cpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_hpp.path};
  target_data.sources = {private_cpp.path};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{cwd,
                                            private_cpp.path,
                                            std::vector<std::string>{"clang",
                                                                     private_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);

  REQUIRE(result.has_value() == true);
  //dump(*result);

  auto output = scanner::merge_includes({*result});
  //dump(*output);
  REQUIRE(output.has_value() == true);

  REQUIRE(output.has_value() == true);
  REQUIRE(output->interface_includes.size() == 1);
  REQUIRE(output->includes.size() == 2);

  CHECK(output->interface_includes[0].path == a_hpp.path);
  CHECK(output->includes[0].path == a_hpp.path);
  CHECK(output->includes[1].path == b_hpp.path);
}

TEST_CASE("scanner: scan does not collect headers included transitively from non-source files",
          "[scanner]")
{
  // Given:
  //         private.cpp 
  //           ╱     ╲
  //   interface.hpp  ╲
  //         ╱         ╲
  //      a.hpp       b.hpp
  //        │           │
  //      x.hpp       y.hpp
  //
  // Expect:
  //   dependencies:           a.hpp and b.hpp
  //   interface dependencies: a.hpp

  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // included from 3rdparty
  Literal_file x_hpp{"/x.hpp", ""};
  Literal_file y_hpp{"/y.hpp", ""};

  // 3rdparty
  Literal_file a_hpp{"/a.hpp", R"(
    #include "x.hpp"
    )"};
  Literal_file b_hpp{"/b.hpp", R"(
    #include "y.hpp"
    )"};

  // interface
  Literal_file interface_hpp{"/interface.hpp", R"(
    #include "a.hpp"
    )"};

  // private
  Literal_file private_cpp{"/private.cpp", R"(
    #include "interface.hpp"
    #include "b.hpp"
    )"};

  add_file(*fs, x_hpp);
  add_file(*fs, y_hpp);
  add_file(*fs, a_hpp);
  add_file(*fs, b_hpp);
  add_file(*fs, interface_hpp);
  add_file(*fs, private_cpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_hpp.path};
  target_data.sources = {private_cpp.path};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{cwd,
                                            private_cpp.path,
                                            std::vector<std::string>{"clang",
                                                                     private_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(result.has_value() == true);
  auto output = scanner::merge_includes({*result});

  REQUIRE(output.has_value() == true);
  REQUIRE(output->interface_includes.size() == 1);
  REQUIRE(output->includes.size() == 2);

  CHECK(output->interface_includes[0].path == a_hpp.path);
  CHECK(output->includes[0].path == a_hpp.path);
  CHECK(output->includes[1].path == b_hpp.path);
}

TEST_CASE("scanner: scan collects headers included transitively from interface headers",
          "[scanner]")
{
  // Given:
  //       private.cpp 
  //           │
  //           │
  //       interface_1.hpp
  //        ╱     ╲
  //       ╱       ╲
  //    a.hpp  interface_2.hpp
  //            ╱     ╲
  //           ╱       ╲
  //        b.hpp  interface_3.hpp
  //                  ╱
  //                 ╱
  //              c.hpp
  //
  // Expect:
  //   dependencies:           a.hpp, b.hpp, and c.hpp
  //   interface dependencies: a.hpp, b.hpp, and c.hpp

  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // 3rdparty
  Literal_file a_hpp{"/a.hpp", ""};
  Literal_file b_hpp{"/b.hpp", ""};
  Literal_file c_hpp{"/c.hpp", ""};

  // interface
  Literal_file interface_1_hpp{"/interface_1.hpp", R"(
    #include "a.hpp"
    #include "interface_2.hpp"
    )"};
  Literal_file interface_2_hpp{"/interface_2.hpp", R"(
    #include "b.hpp"
    #include "interface_3.hpp"
    )"};
  Literal_file interface_3_hpp{"/interface_3.hpp", R"(
    #include "c.hpp"
    )"};

  // private
  Literal_file private_cpp{"/private.cpp", R"(
    #include "interface_1.hpp"
    )"};

  add_file(*fs, a_hpp);
  add_file(*fs, b_hpp);
  add_file(*fs, c_hpp);
  add_file(*fs, interface_1_hpp);
  add_file(*fs, interface_2_hpp);
  add_file(*fs, interface_3_hpp);
  add_file(*fs, private_cpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_1_hpp.path,
                                   interface_2_hpp.path,
                                   interface_3_hpp.path};
  target_data.sources = {private_cpp.path};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{cwd,
                                            private_cpp.path,
                                            std::vector<std::string>{"clang",
                                                                     private_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(result.has_value() == true);
  auto output = scanner::merge_includes({*result});
  REQUIRE(output.has_value() == true);

  REQUIRE(output->interface_includes.size() == 3);
  REQUIRE(output->includes.size() == 3);

  CHECK(output->interface_includes[0].path == a_hpp.path);
  CHECK(output->interface_includes[1].path == b_hpp.path);
  CHECK(output->interface_includes[2].path == c_hpp.path);
  CHECK(output->includes[0].path == a_hpp.path);
  CHECK(output->includes[1].path == b_hpp.path);
  CHECK(output->includes[2].path == c_hpp.path);
}

TEST_CASE("scanner: scan collects headers included transitively from private headers",
          "[scanner]")
{
  // Given:
  //        private.cpp 
  //         ╱      ╲
  //        ╱        ╲
  // interface.hpp  private_1.hpp
  //                  ╱     ╲
  //                 ╱       ╲
  //              a.hpp   private_2.hpp
  //                        ╱    ╲
  //                       ╱      ╲
  //                    b.hpp  private_3.hpp
  //                             ╱
  //                            ╱
  //                         c.hpp
  //
  // Expect:
  //   dependencies:           a.hpp, b.hpp, and c.hpp
  //   interface dependencies:

  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // 3rdparty
  Literal_file a_hpp{"/a.hpp", ""};
  Literal_file b_hpp{"/b.hpp", ""};
  Literal_file c_hpp{"/c.hpp", ""};

  // interface
  Literal_file interface_hpp{"/interface.hpp", ""};

  // private
  Literal_file private_cpp{"/private.cpp", R"(
    #include "interface.hpp"
    #include "private_1.hpp"
    )"};
  Literal_file private_1_hpp{"/private_1.hpp", R"(
    #include "private_2.hpp"
    #include "a.hpp"
    )"};
  Literal_file private_2_hpp{"/private_2.hpp", R"(
    #include "private_3.hpp"
    #include "b.hpp"
    )"};
  Literal_file private_3_hpp{"/private_3.hpp", R"(
    #include "c.hpp"
    )"};

  add_file(*fs, a_hpp);
  add_file(*fs, b_hpp);
  add_file(*fs, c_hpp);
  add_file(*fs, interface_hpp);
  add_file(*fs, private_cpp);
  add_file(*fs, private_1_hpp);
  add_file(*fs, private_2_hpp);
  add_file(*fs, private_3_hpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_hpp.path};
  target_data.sources = {private_cpp.path,
                         private_1_hpp.path,
                         private_2_hpp.path,
                         private_3_hpp.path};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{cwd,
                                            private_cpp.path,
                                            std::vector<std::string>{"clang",
                                                                     private_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(result.has_value() == true);
  auto output = scanner::merge_includes({*result});
  REQUIRE(output.has_value() == true);

  REQUIRE(output.has_value() == true);
  REQUIRE(output->interface_includes.empty());
  REQUIRE(output->includes.size() == 3);

  CHECK(output->includes[0].path == a_hpp.path);
  CHECK(output->includes[1].path == b_hpp.path);
  CHECK(output->includes[2].path == c_hpp.path);
}

TEST_CASE("scanner: scan collects an interface dependency when previously included privately", "[scanner]")
{
  // Given:
  //     private.cpp
  //        ╱   ╲
  //       ╱     ╲
  //       ╲    interface.hpp
  //        ╲    ╱
  //         ╲  ╱
  //        a.hpp
  //
  // Expect:
  //   dependencies:           a.hpp
  //   interface dependencies: a.hpp

  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // 3rdparty
  Literal_file a_hpp{"/a.hpp", R"(
    #ifndef a_hpp_guard
    #define a_hpp_guard
    #endif
    )"};

  // interface
  Literal_file interface_hpp{"/interface.hpp", R"(
    #ifndef interface_hpp_guard
    #define interface_hpp_guard
    #include "a.hpp"
    #endif
    )"};

  // private
  Literal_file private_cpp{"/private.cpp", R"(
    #include "a.hpp"
    #include "interface.hpp"
    )"};

  add_file(*fs, a_hpp);
  add_file(*fs, interface_hpp);
  add_file(*fs, private_cpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_hpp.path};
  target_data.sources = {private_cpp.path};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{cwd,
                                            private_cpp.path,
                                            std::vector<std::string>{"clang",
                                                                     private_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);

  REQUIRE(result.has_value() == true);
  //dump(*result);

  auto output = scanner::merge_includes({*result});
  //dump(*output);
  REQUIRE(output.has_value() == true);

  REQUIRE(output.has_value() == true);
  REQUIRE(output->interface_includes.size() == 1);
  REQUIRE(output->includes.size() == 1);

  CHECK(output->interface_includes[0].path == a_hpp.path);
  CHECK(output->includes[0].path == a_hpp.path);
}

TEST_CASE("scanner: scan collects interface headers when main file is external", "[scanner]")
{
  // Given:
  //     generated.cpp
  //          │
  //     interface.hpp
  //        ╱    ╲
  //     a.hpp  b.hpp
  //
  // Expect:
  //   dependencies:
  //   interface dependencies: a.hpp

  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // 3rdparty
  Literal_file a_hpp{"/a.hpp", ""};
  Literal_file b_hpp{"/b.hpp", ""};

  // interface
  Literal_file interface_hpp{"/interface.hpp", R"(
    #include "a.hpp"
    #include "b.hpp"
    )"};

  // private
  Literal_file generated_cpp{"/generated.cpp", R"(
    #include "interface.hpp"
    )"};

  add_file(*fs, a_hpp);
  add_file(*fs, b_hpp);
  add_file(*fs, interface_hpp);
  add_file(*fs, generated_cpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_hpp.path};
  target_data.sources = {};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{cwd,
                                            generated_cpp.path,
                                            std::vector<std::string>{"clang",
                                                                     generated_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);

  REQUIRE(result.has_value() == true);
  //dump(*result);

  auto output = scanner::merge_includes({*result});
  //dump(*output);
  REQUIRE(output.has_value() == true);

  REQUIRE(output.has_value() == true);
  REQUIRE(output->interface_includes.size() == 2);
  REQUIRE(output->includes.empty());

  CHECK(output->interface_includes[0].path == a_hpp.path);
  CHECK(output->interface_includes[1].path == b_hpp.path);
}

TEST_CASE("scanner: scan ignores interface headers included via a cycle", "[scanner]")
{
  // Given:
  //     private.cpp
  //          │
  //        a.hpp
  //          │
  //     interface.hpp
  //          │
  //        b.hpp
  //
  // Expect:
  //   dependencies: a.hpp
  //   interface dependencies:

  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // 3rdparty
  Literal_file a_hpp{"/a.hpp", R"(
    #include "interface.hpp"
    )"};
  Literal_file b_hpp{"/b.hpp", ""};

  // interface
  Literal_file interface_hpp{"/interface.hpp", R"(
    #include "b.hpp"
    )"};

  // private
  Literal_file private_cpp{"/private.cpp", R"(
    #include "a.hpp"
    )"};

  add_file(*fs, a_hpp);
  add_file(*fs, b_hpp);
  add_file(*fs, interface_hpp);
  add_file(*fs, private_cpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_hpp.path};
  target_data.sources = {private_cpp.path};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{cwd,
                                            private_cpp.path,
                                            std::vector<std::string>{"clang",
                                                                     private_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);

  REQUIRE(result.has_value() == true);
  //dump(*result);

  auto output = scanner::merge_includes({*result});
  //dump(*output);
  REQUIRE(output.has_value() == true);

  REQUIRE(output.has_value() == true);
  REQUIRE(output->interface_includes.empty());
  REQUIRE(output->includes.size() == 1);

  CHECK(output->includes[0].path == a_hpp.path);
}

TEST_CASE("scanner: scan can distinguish private sources in the interface include directory",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);
  auto fs = llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};

  // 3rdparty
  Literal_file a_hpp{"/opt/a.hpp", ""};
  Literal_file b_hpp{"/opt/b.hpp", ""};

  // interface
  Literal_file interface_hpp{"/src/interface.hpp", R"(
    #include "a.hpp"
    )"};

  // private
  Literal_file private_cpp{"/src/private.cpp", R"(
    #include "interface.hpp"
    #include "b.hpp"
    )"};

  add_file(*fs, a_hpp);
  add_file(*fs, b_hpp);
  add_file(*fs, interface_hpp);
  add_file(*fs, private_cpp);

  target_model::Target_data target_data;
  target_data.interface_headers = {interface_hpp.path};
  target_data.interface_include_directories = {"/src"};
  target_data.sources = {private_cpp.path};

  std::filesystem::path cwd{"/"};
  scanner::Compile_command compile_commands{
    cwd,
    private_cpp.path,
    std::vector<std::string>{"clang", "-I/src", "-I/opt", private_cpp.path}};

  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;

  auto result = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);

  REQUIRE(result.has_value() == true);
  //dump(*result);

  auto output = scanner::merge_includes({*result});
  //dump(*output);
  REQUIRE(output.has_value() == true);

  REQUIRE(output.has_value() == true);
  REQUIRE(output->interface_includes.size() == 1);
  REQUIRE(output->includes.size() == 2);

  CHECK(output->interface_includes[0].path == a_hpp.path);
  CHECK(output->includes[0].path == a_hpp.path);
  CHECK(output->includes[1].path == b_hpp.path);
}
