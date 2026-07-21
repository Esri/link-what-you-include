// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/load_config.hpp>

#include <target_model/target.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <system_error>

namespace
{
class Temp_file
{
public:
  explicit Temp_file(const std::string& content)
  : path_(std::filesystem::temp_directory_path() /
          std::filesystem::path{std::format("lwyi-load-config-test-{}.json",
                                            std::hash<std::string>{}(content))})
  {
    std::ofstream output(path_);
    output << content;
  }

  Temp_file(const Temp_file&) = delete;
  Temp_file& operator=(const Temp_file&) = delete;
  Temp_file(Temp_file&&) = delete;
  Temp_file& operator=(Temp_file&&) = delete;

  ~Temp_file()
  {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  [[nodiscard]] const std::filesystem::path& path() const
  {
    return path_;
  }

private:
  std::filesystem::path path_;
};
} // namespace

TEST_CASE("lwyi: load_config loads target overrides", "[lwyi]")
{
  Temp_file file{R"({
  "targets": {
    "liba": {
      "skip_validation": true,
      "interface_include_prefixes": ["foo", "bar"]
    },
    "libb": {
      "skip_validation": false,
      "interface_include_prefixes": []
    }
  }
})"};

  auto result = lwyi::load_config(file.path());
  REQUIRE(result.has_value());

  const auto& config = result.value();
  REQUIRE(config.targets.size() == 2);

  const auto it_a = config.targets.find(target_model::Target{"liba"});
  REQUIRE(it_a != config.targets.end());
  CHECK(it_a->second.skip_validation);
  CHECK(it_a->second.interface_include_prefixes == std::set<std::string>{"bar", "foo"});

  const auto it_b = config.targets.find(target_model::Target{"libb"});
  REQUIRE(it_b != config.targets.end());
  CHECK(!it_b->second.skip_validation);
  CHECK(it_b->second.interface_include_prefixes.empty());
}

TEST_CASE("lwyi: load_config succeeds when targets is absent", "[lwyi]")
{
  Temp_file file{"{}"};

  auto result = lwyi::load_config(file.path());
  REQUIRE(result.has_value());
  CHECK(result->targets.empty());
}

TEST_CASE("lwyi: load_config defaults missing target fields", "[lwyi]")
{
  Temp_file file{R"({
  "targets": {
    "liba": {}
  }
})"};

  auto result = lwyi::load_config(file.path());
  REQUIRE(result.has_value());

  const auto it = result->targets.find(target_model::Target{"liba"});
  REQUIRE(it != result->targets.end());
  CHECK(!it->second.skip_validation);
  CHECK(it->second.interface_include_prefixes.empty());
}

TEST_CASE("lwyi: load_config fails for invalid skip_validation type", "[lwyi]")
{
  Temp_file file{R"({
  "targets": {
    "liba": {
      "skip_validation": "true"
    }
  }
})"};

  auto result = lwyi::load_config(file.path());
  REQUIRE(!result.has_value());
  CHECK(result.error().find("Failed to parse config file") != std::string::npos);
}

TEST_CASE("lwyi: load_config fails for invalid interface_include_prefixes type", "[lwyi]")
{
  Temp_file file{R"({
  "targets": {
    "liba": {
      "interface_include_prefixes": "foo"
    }
  }
})"};

  auto result = lwyi::load_config(file.path());
  REQUIRE(!result.has_value());
  CHECK(result.error().find("Failed to parse config file") != std::string::npos);
}

TEST_CASE("lwyi: load_config fails for invalid JSON", "[lwyi]")
{
  Temp_file file{"{"};

  auto result = lwyi::load_config(file.path());
  REQUIRE(!result.has_value());
  CHECK(result.error().find("Failed to parse config file") != std::string::npos);
}
