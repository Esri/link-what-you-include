// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>
#include <target_model/target_model_loader.hpp>
#include <target_model/target_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

namespace
{
class Temp_directory
{
public:
  explicit Temp_directory(std::string_view name)
  : path_(std::filesystem::temp_directory_path() /
          std::filesystem::path{name})
  {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    std::filesystem::create_directories(path_ / ".cmake" / "api" / "v1" / "reply");
  }

  Temp_directory(const Temp_directory&) = delete;
  Temp_directory& operator=(const Temp_directory&) = delete;
  Temp_directory(Temp_directory&&) = delete;
  Temp_directory& operator=(Temp_directory&&) = delete;

  ~Temp_directory()
  {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  [[nodiscard]] const std::filesystem::path& path() const
  {
    return path_;
  }


  // NOLINTBEGIN(bugprone-easily-swappable-parameters)
  void write(std::string_view path_suffix, std::string_view file_content) const
  {
    const auto path = path_ / std::filesystem::path(path_suffix);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << file_content;
  }
  // NOLINTEND(bugprone-easily-swappable-parameters)

private:
  std::filesystem::path path_;
};
} // namespace

TEST_CASE("target_model: file_api_target_model_loader_impl can load a valid codemodel reply",
          "[target_model]")
{
  Temp_directory temp_directory{"lwyi-file-api-test-valid"};
  const auto& build_dir = temp_directory.path();

  temp_directory.write(".cmake/api/v1/reply/codemodel-v2-123.json",
                       R"===({
  "version": { "major": 2, "minor": 10 },
  "paths": { "source": "/some/source" },
  "configurations": [
    {
      "targets": [
        { "id": "liba::@1", "name": "liba", "jsonFile": "target-liba.json" },
        { "id": "liba_verify_interface_header_sets::@1", "name": "liba_verify_interface_header_sets", "jsonFile": "target-liba-verify.json" },
        { "id": "libb::@1", "name": "libb", "jsonFile": "target-libb.json" },
        { "id": "libinterface::@1", "name": "libinterface", "jsonFile": "target-libinterface.json" }
      ]
    }
  ]
})===");
  temp_directory.write(".cmake/api/v1/reply/target-liba.json",
                       R"===({
  "id": "liba::@1",
  "name": "liba",
  "type": "STATIC_LIBRARY",
  "fileSets": [
    { "type": "HEADERS", "visibility": "PUBLIC" }
  ],
  "backtraceGraph": { "commands": [], "files": [], "nodes": [] },
  "sources": [
    { "path": "/some/source/liba.cpp" },
    { "path": "/some/source/include/liba/liba.h", "fileSetIndexes": [0] }
  ],
  "compileDependencies": []
})===");
  temp_directory.write(".cmake/api/v1/reply/target-liba-verify.json",
                       R"===({
  "id": "liba_verify_interface_header_sets::@1",
  "name": "liba_verify_interface_header_sets",
  "type": "EXECUTABLE",
  "sources": [
    { "path": "/some/build/liba_verify_interface_header_sets/liba/liba.h.cxx" }
  ],
  "compileDependencies": []
})===");
  temp_directory.write(".cmake/api/v1/reply/target-libb.json",
                       R"===({
  "id": "libb::@1",
  "name": "libb",
  "type": "STATIC_LIBRARY",
  "fileSets": [],
  "backtraceGraph": { "commands": [], "files": [], "nodes": [] },
  "sources": [
    { "path": "/some/source/libb.cpp" }
  ],
  "interfaceCompileDependencies": [
    { "id": "liba::@1" },
    { "id": "libinterface::@1" }
  ],
  "compileDependencies": [
    { "id": "liba::@1" }
  ]
})===");
  temp_directory.write(".cmake/api/v1/reply/target-libinterface.json",
                       R"===({
  "id": "libinterface::@1",
  "name": "libinterface",
  "type": "STATIC_LIBRARY",
  "fileSets": [],
  "backtraceGraph": { "commands": [], "files": [], "nodes": [] },
  "sources": [
    { "path": "/some/source/libinterface.cpp" }
  ],
  "compileDependencies": []
})===");
  auto loader = target_model::Target_model_loader::create();
  auto result = loader->load_directory(build_dir);
  if (!result.has_value())
  {
    FAIL(result.error());
  }

  auto target_model = loader->make_target_model();

  auto data = target_model.get_target_data(target_model::Target{"liba"});
  if (!data) {
    FAIL("Target data not found for liba");
  }
  REQUIRE(data.has_value());
  const auto& liba = data->get(); // NOLINT(bugprone-unchecked-optional-access)
  CHECK(liba.interface_include_directories.empty());
  CHECK(liba.interface_headers ==
        std::unordered_set<std::filesystem::path>{"/some/source/include/liba/liba.h"});
  CHECK(liba.sources == std::unordered_set<std::filesystem::path>{"/some/source/liba.cpp",
                                                                  "/some/source/include/liba/liba.h"});
  CHECK(liba.verify_interface_header_sets_sources ==
        std::unordered_set<std::filesystem::path>{"/some/build/liba_verify_interface_header_sets/liba/liba.h.cxx"});
  CHECK(liba.dependencies.empty());
  CHECK(liba.interface_dependencies.empty());

  data = target_model.get_target_data(target_model::Target{"libb"});
  if (!data) {
    FAIL("Target data not found for libb");
  }
  REQUIRE(data.has_value());
  const auto& libb = data->get(); // NOLINT(bugprone-unchecked-optional-access)
  CHECK(libb.interface_include_directories.empty());
  CHECK(libb.dependencies == std::unordered_set<target_model::Target>{{"liba"}});
  CHECK(libb.interface_dependencies ==
        std::unordered_set<target_model::Target>{{"liba"}, {"libinterface"}});
}

TEST_CASE("target_model: file_api_target_model_loader_impl get dependency backtrace",
          "[target_model]")
{
  Temp_directory temp_directory{"lwyi-file-api-test-dependency-backtrace"};
  const auto& build_dir = temp_directory.path();

  temp_directory.write(".cmake/api/v1/reply/codemodel-v2-123.json",
                       R"===({
  "version": { "major": 2, "minor": 10 },
  "paths": { "source": "/some/source" },
  "configurations": [
    {
      "targets": [
        { "id": "liba::@1", "name": "liba", "jsonFile": "target-liba.json" },
        { "id": "libb::@1", "name": "libb", "jsonFile": "target-libb.json" }
      ]
    }
  ]
})===");
  temp_directory.write(".cmake/api/v1/reply/target-liba.json",
                       R"===({
  "id": "liba::@1",
  "name": "liba",
  "type": "STATIC_LIBRARY",
  "fileSets": [],
  "backtraceGraph": { "commands": [], "files": [], "nodes": [] },
  "sources": [
    { "path": "/some/source/liba.cpp" }
  ],
  "compileDependencies": []
})===");
  temp_directory.write(".cmake/api/v1/reply/target-libb.json",
                       R"===({
  "id": "libb::@1",
  "name": "libb",
  "type": "STATIC_LIBRARY",
  "fileSets": [],
  "backtraceGraph": {
    "commands": ["target_link_libraries"],
    "files": ["lib/CMakeLists.txt"],
    "nodes": [
      { "file": 0 },
      { "command": 0, "file": 0, "line": 21, "parent": 0 }
    ]
  },
  "sources": [
    { "path": "/some/source/libb.cpp" }
  ],
  "compileDependencies": [
    { "id": "liba::@1", "backtrace": 1 }
  ],
  "dependencies": [
    { "id": "liba::@1", "backtrace": 1 }
  ]
})===");

  auto loader = target_model::Target_model_loader::create();
  auto result = loader->load_directory(build_dir);
  if (!result.has_value())
  {
    FAIL(result.error());
  }

  auto target_model = loader->make_target_model();
  auto data = target_model.get_target_data(target_model::Target{"libb"});
  if (!data)
  {
    FAIL("Target data not found for libb");
  }
  REQUIRE(data.has_value());

  const auto& libb = data->get(); // NOLINT(bugprone-unchecked-optional-access)
  auto location = libb.dependency_locations.find(target_model::Target{"liba"});
  REQUIRE(location != libb.dependency_locations.end());
  CHECK(location->second.file == std::filesystem::path{"lib/CMakeLists.txt"});
  CHECK(location->second.line == 21);
}

TEST_CASE("target_model: file_api_target_model_loader_impl fails for missing codemodel reply",
          "[target_model]")
{
  Temp_directory temp_directory{"lwyi-file-api-test-missing-codemodel"};
  auto loader = target_model::Target_model_loader::create();
  auto result = loader->load_directory(temp_directory.path());
  REQUIRE(!result.has_value());
  CHECK((result.error().find("could not locate codemodel reply") != std::string::npos ||
         result.error().find("failed to locate CMake File API reply directory") != std::string::npos));
}
