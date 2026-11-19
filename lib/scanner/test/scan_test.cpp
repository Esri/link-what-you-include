// Copyright (c) 2025 Environmental Systems Research Institute, Inc.

// SPDX-License-Identifier: Apache-2.0

#include <scanner/scan.hpp>

#include <message/message.hpp>
#include <scanner/include.hpp>
#include <src/include_sets.hpp>
#include <src/scan_impl.hpp>
#include <target_model/target_data.hpp>

#include <catch2/catch_test_macros.hpp>
#include <clang/Tooling/DependencyScanning/DependencyScanningFilesystem.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <ostream>
#include <unordered_set>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

namespace scanner
{
std::ostream& operator<<(std::ostream& os, const scanner::Source_line& sl)
{
  return os << sl.source.string() << ':' << sl.line;
}

std::ostream& operator<<(std::ostream& os, const scanner::Include_sets& is)
{
  os << "interface includes:";
  for (const auto& interface_include : is.interface_includes)
  {
    os << "\n  " << interface_include.path.string();
    for (const auto& source_line : interface_include.include_chain)
    {
      os << "\n    " << source_line;
    }
  }

  os << "\nincludes:";
  for (const auto& include : is.includes)
  {
    os << "\n  " << include.path.string();
    for (const auto& source_line : include.include_chain)
    {
      os << "\n    " << source_line;
    }
  }

  return os;
}
} // namespace scanner

namespace
{
llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> make_file_system()
{
  return llvm::IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem>{
    new llvm::vfs::InMemoryFileSystem};
}

void add_file(llvm::vfs::InMemoryFileSystem& fs, const char* path, const char* content)
{
  fs.addFile(path, 0, llvm::MemoryBuffer::getMemBuffer(content));
}
} // namespace

TEST_CASE("scanner: scan collects interface and private includes", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // GIVEN:
  //         private.cpp
  //           ╱     ╲
  //   interface.hpp  ╲
  //         ╱         ╲
  //      a.hpp       b.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", "");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/interface.hpp", R"(
    #include "a.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "interface.hpp"
    #include "b.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface.hpp"};
  target_data.sources = {"/private.cpp"};

  // EXPECT:
  //   interface includes: a.hpp
  //   includes:           a.hpp, b.hpp
  scanner::Include_sets expected{
    .interface_includes = {{"/a.hpp", {{"/interface.hpp", 2}}}},
    .includes = {{"/a.hpp", {{"/private.cpp", 2}, {"/interface.hpp", 2}}},
                 {"/b.hpp", {{"/private.cpp", 3}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan does not collect all transitive includes", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //         private.cpp
  //           ╱    ╲
  //   interface.hpp ╲
  //         ╱        ╲
  //      a.hpp      b.hpp
  //         ╲        ╱
  //        x.hpp  y.hpp
  //           ╲    ╱
  //            z.hpp
  auto fs = make_file_system();
  add_file(*fs, "/x.hpp", R"(
    #include "z.hpp"
    )");
  add_file(*fs, "/y.hpp", R"(
    #include "z.hpp"
    )");
  add_file(*fs, "/z.hpp", R"(
    #ifndef z_hpp_guard
    #define z_hpp_guard
    #endif
    )");
  add_file(*fs, "/a.hpp", R"(
    #include "x.hpp"
    )");
  add_file(*fs, "/b.hpp", R"(
    #include "y.hpp"
    )");
  add_file(*fs, "/interface.hpp", R"(
    #include "a.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "interface.hpp"
    #include "b.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface.hpp"};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   interface includes: a.hpp
  //   includes:           a.hpp, b.hpp
  scanner::Include_sets expected{
    .interface_includes = {{"/a.hpp", {{"/interface.hpp", 2}}}},
    .includes = {{"/a.hpp", {{"/private.cpp", 2}, {"/interface.hpp", 2}}},
                 {"/b.hpp", {{"/private.cpp", 3}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects tranitive interface includes", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //      private.cpp
  //          │
  //          │
  //      interface_1.hpp
  //       ╱     ╲
  //      ╱       ╲
  //   a.hpp  interface_2.hpp
  //            ╱    ╲
  //           ╱      ╲
  //        b.hpp  interface_3.hpp
  //                  ╱
  //                 ╱
  //              c.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", "");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/c.hpp", "");
  add_file(*fs, "/interface_1.hpp", R"(
    #include "a.hpp"
    #include "interface_2.hpp"
    )");
  add_file(*fs, "/interface_2.hpp", R"(
    #include "b.hpp"
    #include "interface_3.hpp"
    )");
  add_file(*fs, "/interface_3.hpp", R"(
    #include "c.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "interface_1.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface_1.hpp", "/interface_2.hpp", "/interface_3.hpp"};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   dependencies:           a.hpp, b.hpp, c.hpp
  //   interface dependencies: a.hpp, b.hpp, c.hpp
  scanner::Include_sets expected{
    .interface_includes = {{"/a.hpp", {{"/interface_1.hpp", 2}}},
                           {"/b.hpp", {{"/interface_2.hpp", 2}}},
                           {"/c.hpp", {{"/interface_3.hpp", 2}}}},
    .includes = {{"/a.hpp", {{"/private.cpp", 2}, {"/interface_1.hpp", 2}}},
                 {"/b.hpp",
                  {{"/private.cpp", 2}, {"/interface_1.hpp", 3}, {"/interface_2.hpp", 2}}},
                 {"/c.hpp",
                  {{"/private.cpp", 2},
                   {"/interface_1.hpp", 3},
                   {"/interface_2.hpp", 3},
                   {"/interface_3.hpp", 2}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects transitive private includes", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //          private.cpp
  //               │
  //               │
  //          private_1.hpp
  //            ╱     ╲
  //           ╱       ╲
  //        a.hpp   private_2.hpp
  //                  ╱    ╲
  //                 ╱      ╲
  //              b.hpp  private_3.hpp
  //                       ╱
  //                      ╱
  //                   c.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", "");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/c.hpp", "");
  add_file(*fs, "/private.cpp", R"(
    #include "private_1.hpp"
    )");
  add_file(*fs, "/private_1.hpp", R"(
    #include "private_2.hpp"
    #include "a.hpp"
    )");
  add_file(*fs, "/private_2.hpp", R"(
    #include "private_3.hpp"
    #include "b.hpp"
    )");
  add_file(*fs, "/private_3.hpp", R"(
    #include "c.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {};
  target_data.sources = {"/private.cpp", "/private_1.hpp", "/private_2.hpp", "/private_3.hpp"};

  // Expect:
  //   dependencies:           a.hpp, b.hpp, and c.hpp
  //   interface dependencies:
  scanner::Include_sets expected{
    .interface_includes = {},
    .includes = {
      {"/a.hpp", {{"/private.cpp", 2}, {"/private_1.hpp", 3}}},
      {"/b.hpp", {{"/private.cpp", 2}, {"/private_1.hpp", 2}, {"/private_2.hpp", 3}}},
      {"/c.hpp",
       {{"/private.cpp", 2}, {"/private_1.hpp", 2}, {"/private_2.hpp", 2}, {"/private_3.hpp", 2}}},
    }};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects interface includes externally included", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //   private.cpp
  //        │
  //      a.hpp
  //        │
  //   interface.hpp
  //        │
  //      b.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #include "interface.hpp"
    )");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/interface.hpp", R"(
    #include "b.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface.hpp"};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   dependencies: a.hpp
  //   interface dependencies: b.hpp
  scanner::Include_sets expected{.interface_includes = {{"/b.hpp", {{"/interface.hpp", 2}}}},
                                 .includes = {{"/a.hpp", {{"/private.cpp", 2}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan does not collect private includes externally included", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // TODO: scan should produce an error when including private_1.hpp from a.hpp

  // Given:
  //   private.cpp
  //        │
  //      a.hpp
  //        │
  //   private_1.hpp
  //        │
  //      b.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #include "private_1.hpp"
    )");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/private_1.hpp", R"(
    #include "b.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {};
  target_data.sources = {"/private.cpp", "/private_1.hpp"};

  // Expect:
  //   dependencies: a.hpp
  //   interface dependencies:
  scanner::Include_sets expected{.interface_includes = {},
                                 .includes = {{"/a.hpp", {{"/private.cpp", 2}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects private includes previously externally included", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //    private.cpp
  //       ╱   ╲
  //      ╱     ╲
  //   a.hpp   b.hpp
  //     │
  //   b.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #include "b.hpp"
    )");
  add_file(*fs, "/b.hpp", R"(
    #ifndef b_hpp_guard
    #define b_hpp_guard
    #endif
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    #include "b.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   interface includes:
  //   includes:           a.hpp b.hpp
  scanner::Include_sets expected{.interface_includes = {},
                                 .includes = {{"/a.hpp", {{"/private.cpp", 2}}},
                                              {"/b.hpp", {{"/private.cpp", 3}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects interface includes previously externally included",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //   private.cpp
  //       ╱   ╲
  //      ╱     ╲
  //   a.hpp  interface.hpp
  //      ╲     ╱
  //       ╲   ╱
  //       b.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #include "b.hpp"
    )");
  add_file(*fs, "/b.hpp", R"(
    #ifndef b_hpp_guard
    #define b_hpp_guard
    #endif
    )");
  add_file(*fs, "/interface.hpp", R"(
    #include "b.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    #include "interface.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface.hpp"};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   interface includes: b.hpp
  //   includes:           a.hpp b.hpp
  scanner::Include_sets expected{.interface_includes = {{"/b.hpp", {{"/interface.hpp", 2}}}},
                                 .includes = {
                                   {"/a.hpp", {{"/private.cpp", 2}}},
                                   {"/b.hpp", {{"/private.cpp", 3}, {"/interface.hpp", 2}}},
                                 }};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects interface includes previously privately included",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //   private.cpp
  //      ╱   ╲
  //     ╱     ╲
  //     ╲    interface.hpp
  //      ╲    ╱
  //       ╲  ╱
  //      a.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", "");
  add_file(*fs, "/interface.hpp", R"(
    #ifndef interface_hpp_guard
    #define interface_hpp_guard
    #include "a.hpp"
    #endif
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    #include "interface.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface.hpp"};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   interface includes: a.hpp
  //   includes:           a.hpp
  scanner::Include_sets expected{.interface_includes = {{"/a.hpp", {{"/interface.hpp", 4}}}},
                                 .includes = {{"/a.hpp", {{"/private.cpp", 2}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects transitive interface includes previously externally included",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //      private.cpp
  //        ╱    ╲
  //      a.hpp   ╲
  //        ╲     ╱
  //     interface_1.hpp
  //       ╱     ╲
  //      ╱       ╲
  //   b.hpp  interface_2.hpp
  //           ╱     ╲
  //          ╱       ╲
  //       c.hpp  interface_3.hpp
  //                 ╱
  //                ╱
  //             d.hpp
  //
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #include "interface_1.hpp"
    )");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/c.hpp", "");
  add_file(*fs, "/d.hpp", "");
  add_file(*fs, "/interface_3.hpp", R"(
    #include "d.hpp"
    )");
  add_file(*fs, "/interface_2.hpp", R"(
    #include "c.hpp"
    #include "interface_3.hpp"
    )");
  add_file(*fs, "/interface_1.hpp", R"(
    #ifndef interface_1_hpp_guard
    #define interface_1_hpp_guard
    #include "b.hpp"
    #include "interface_2.hpp"
    #endif
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    #include "interface_1.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface_1.hpp", "/interface_2.hpp", "/interface_3.hpp"};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   interface includes: b.hpp c.hpp d.hpp
  //   includes:           a.hpp b.hpp c.hpp d.hpp
  scanner::Include_sets expected{
    .interface_includes = {{"/b.hpp", {{"/interface_1.hpp", 4}}},
                           {"/c.hpp", {{"/interface_2.hpp", 2}}},
                           {"/d.hpp", {{"/interface_3.hpp", 2}}}},
    .includes = {{"/a.hpp", {{"/private.cpp", 2}}},
                 {"/b.hpp", {{"/private.cpp", 3}, {"/interface_1.hpp", 4}}},
                 {"/c.hpp",
                  {{"/private.cpp", 3}, {"/interface_1.hpp", 5}, {"/interface_2.hpp", 2}}},
                 {"/d.hpp",
                  {{"/private.cpp", 3},
                   {"/interface_1.hpp", 5},
                   {"/interface_2.hpp", 3},
                   {"/interface_3.hpp", 2}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects transitive interface includes previously externally included v2",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //      private.cpp
  //        ╱    ╲
  //     a.hpp    ╲
  //       │       ╲
  //       │   interface_1.hpp
  //       │        │
  //       │        │
  //       │   interface_2.hpp
  //       │       ╱
  //       │      ╱
  //     interface_3.hpp
  //          │
  //          │
  //         b.hpp
  //
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #include "interface_3.hpp"
    )");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/interface_3.hpp", R"(
    #ifndef interface_3_hpp_guard
    #define interface_3_hpp_guard
    #include "b.hpp"
    #endif
    )");
  add_file(*fs, "/interface_2.hpp", R"(
    #include "interface_3.hpp"
    )");
  add_file(*fs, "/interface_1.hpp", R"(
    #include "interface_2.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    #include "interface_1.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface_1.hpp", "/interface_2.hpp", "/interface_3.hpp"};
  target_data.sources = {"/private.cpp"};

  // Expect:
  //   interface includes: b.hpp
  //   includes:           a.hpp b.hpp
  scanner::Include_sets expected{.interface_includes = {{"/b.hpp", {{"/interface_3.hpp", 4}}}},
                                 .includes = {{"/a.hpp", {{"/private.cpp", 2}}},
                                              {"/b.hpp",
                                               {{"/private.cpp", 3},
                                                {"/interface_1.hpp", 2},
                                                {"/interface_2.hpp", 2},
                                                {"/interface_3.hpp", 4}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects transitive private includes previously externally included",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // TODO: scan should produce an error when including private_1.hpp from a.hpp

  // Given:
  //      private.cpp
  //        ╱    ╲
  //      a.hpp   ╲
  //        ╲     ╱
  //     private_1.hpp
  //       ╱     ╲
  //      ╱       ╲
  //   b.hpp  private_2.hpp
  //           ╱     ╲
  //          ╱       ╲
  //       c.hpp  private_3.hpp
  //                 ╱
  //                ╱
  //             d.hpp
  //
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #include "private_1.hpp"
    )");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/c.hpp", "");
  add_file(*fs, "/d.hpp", "");
  add_file(*fs, "/private_3.hpp", R"(
    #include "d.hpp"
    )");
  add_file(*fs, "/private_2.hpp", R"(
    #include "c.hpp"
    #include "private_3.hpp"
    )");
  add_file(*fs, "/private_1.hpp", R"(
    #ifndef private_1_hpp_guard
    #define private_1_hpp_guard
    #include "b.hpp"
    #include "private_2.hpp"
    #endif
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    #include "private_1.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {};
  target_data.sources = {"/private.cpp", "/private_1.hpp", "/private_2.hpp", "/private_3.hpp"};

  // Expect:
  //   interface includes: b.hpp c.hpp d.hpp
  //   includes:           a.hpp b.hpp c.hpp d.hpp
  scanner::Include_sets expected{
    .interface_includes = {},
    .includes = {{"/a.hpp", {{"/private.cpp", 2}}},
                 {"/b.hpp", {{"/private.cpp", 3}, {"/private_1.hpp", 4}}},
                 {"/c.hpp",
                  {{"/private.cpp", 3}, {"/private_1.hpp", 5}, {"/private_2.hpp", 2}}},
                 {"/d.hpp",
                  {{"/private.cpp", 3},
                   {"/private_1.hpp", 5},
                   {"/private_2.hpp", 3},
                   {"/private_3.hpp", 2}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects transitive interface includes through a so-called private header",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // TODO: scan should produce an error when including private_1.hpp from interface_1.hpp

  // Given:
  //      private.cpp
  //          │
  //          │
  //      interface_1.hpp
  //       ╱     ╲
  //      ╱       ╲
  //   a.hpp  private_1.hpp
  //            ╱    ╲
  //           ╱      ╲
  //        b.hpp   private_2.hpp
  //                  ╱
  //                 ╱
  //              c.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", "");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/c.hpp", "");
  add_file(*fs, "/interface_1.hpp", R"(
    #include "a.hpp"
    #include "private_1.hpp"
    )");
  add_file(*fs, "/private_1.hpp", R"(
    #include "b.hpp"
    #include "private_2.hpp"
    )");
  add_file(*fs, "/private_2.hpp", R"(
    #include "c.hpp"
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "interface_1.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface_1.hpp"};
  target_data.sources = {"/private.cpp", "/private_1.hpp", "/private_2.hpp"};

  // Expect:
  //   dependencies:           a.hpp, b.hpp, c.hpp
  //   interface dependencies: a.hpp, b.hpp, c.hpp
  scanner::Include_sets expected{
    .interface_includes =
      {{"/a.hpp", {{"/interface_1.hpp", 2}}},
       {"/b.hpp", {{"/interface_1.hpp", 3}, {"/private_1.hpp", 2}}},
       {"/c.hpp", {{"/interface_1.hpp", 3}, {"/private_1.hpp", 3}, {"/private_2.hpp", 2}}}},
    .includes = {{"/a.hpp", {{"/private.cpp", 2}, {"/interface_1.hpp", 2}}},
                 {"/b.hpp",
                  {{"/private.cpp", 2}, {"/interface_1.hpp", 3}, {"/private_1.hpp", 2}}},
                 {"/c.hpp",
                  {{"/private.cpp", 2},
                   {"/interface_1.hpp", 3},
                   {"/private_1.hpp", 3},
                   {"/private_2.hpp", 2}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan sees all dependencies from a file included in multiple contexts",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //           private.cpp
  //             ╱    ╲
  //          a.hpp    ╲
  //           ╱        ╲
  //   interface.hpp   interface.hpp
  //          │          │
  //        b.hpp      c.hpp
  // Where:
  //   interface.hpp does not have include guards and has different dependencies based on preprocessor definitions
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", R"(
    #ifndef a_hpp_guard
    #define a_hpp_guard
    #include "interface.hpp"
    #endif
    )");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/c.hpp", "");
  add_file(*fs, "/interface.hpp", R"(
    #ifdef INTERNAL
    #  include "c.hpp"
    #else
    #  include "b.hpp"
    #endif
    )");
  add_file(*fs, "/private.cpp", R"(
    #include "a.hpp"
    #define INTERNAL 1
    #include "interface.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface.hpp"};
  target_data.sources = {"/private.cpp"};

  // Probabal Expectation (TODO: is support for this possible?):
  //   interface includes: b.hpp
  //   includes:           a.hpp c.hpp
  //
  // Current behavior:
  //   interface includes: b.hpp c.hpp
  //   includes:           a.hpp b.hpp c.hpp
  scanner::Include_sets expected{
    .interface_includes = {{"/b.hpp", {{"/interface.hpp", 5}}},
                           {"/c.hpp", {{"/interface.hpp", 3}}}},
    .includes = {{"/a.hpp", {{"/private.cpp", 2}}},
                 {"/b.hpp", {{"/private.cpp", 4}, {"/interface.hpp", 5}}},
                 {"/c.hpp", {{"/private.cpp", 4}, {"/interface.hpp", 3}}}}};

  scanner::Compile_command compile_commands{"/", "/private.cpp", {"clang", "/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan collects interface headers when main file is external", "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // Given:
  //   generated.cpp
  //        │
  //   interface.hpp
  //      ╱    ╲
  //   a.hpp  b.hpp
  auto fs = make_file_system();
  add_file(*fs, "/a.hpp", "");
  add_file(*fs, "/b.hpp", "");
  add_file(*fs, "/interface.hpp", R"(
    #include "a.hpp"
    #include "b.hpp"
    )");
  add_file(*fs, "/generated.cpp", R"(
    #include "interface.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/interface.hpp"};
  target_data.sources = {};

  // Expect:
  //   dependencies:
  //   interface dependencies: a.hpp, b.hpp
  scanner::Include_sets expected{.interface_includes = {{"/a.hpp", {{"/interface.hpp", 2}}},
                                                        {"/b.hpp", {{"/interface.hpp", 3}}}},
                                 .includes = {}};

  scanner::Compile_command compile_commands{"/", "/generated.cpp", {"clang", "/generated.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

TEST_CASE("scanner: scan can distinguish private sources in the interface include directory",
          "[scanner]")
{
  message::configure(message::Color_output::never, message::Message_level::normal);

  // GIVEN:
  //         private.cpp
  //           ╱     ╲
  //   interface.hpp  ╲
  //         ╱         ╲
  //      a.hpp       b.hpp

  auto fs = make_file_system();
  add_file(*fs, "/opt/a.hpp", "");
  add_file(*fs, "/opt/b.hpp", "");
  add_file(*fs, "/src/interface.hpp", R"(
    #include "a.hpp"
    )");
  add_file(*fs, "/src/private.cpp", R"(
    #include "interface.hpp"
    #include "b.hpp"
    )");
  target_model::Target_data target_data;
  target_data.interface_headers = {"/src/interface.hpp"};
  target_data.interface_include_directories = {"/src"};
  target_data.sources = {"/src/private.cpp"};

  // EXPECT:
  //   interface includes: a.hpp
  //   includes:           a.hpp, b.hpp
  scanner::Include_sets expected{
    .interface_includes = {{"/opt/a.hpp", {{"/src/interface.hpp", 2}}}},
    .includes = {{"/opt/a.hpp", {{"/src/private.cpp", 2}, {"/src/interface.hpp", 2}}},
                 {"/opt/b.hpp", {{"/src/private.cpp", 3}}}}};

  scanner::Compile_command compile_commands{"/",
                                            "/src/private.cpp",
                                            {"clang", "-I/src", "-I/opt", "/src/private.cpp"}};
  clang::tooling::dependencies::DependencyScanningFilesystemSharedCache dep_cache;
  auto output = scanner::scan_impl(fs, dep_cache, target_data, compile_commands);
  REQUIRE(output.has_value() == true);
  CHECK(expected == output->include_sets);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
