// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <target_model/target_model.hpp>

#include <target_model/target.hpp>
#include <target_model/target_data.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <utility>

TEST_CASE("target_model: targets with the same interface include directory are invalid", "[target_model]")
{
  target_model::Target_data liba_target_data;
  target_model::Target_data libb_target_data;
  liba_target_data.interface_include_directories = {"/some/include", "/some/other/include"};
  libb_target_data.interface_include_directories = {"/some/other/include"};

  std::map<target_model::Target, target_model::Target_data> target_to_target_data{
    {{"liba"}, liba_target_data},
    {{"libb"}, libb_target_data}};
  target_model::Target_model target_model{std::move(target_to_target_data)};

  const auto result = target_model.validate();
  REQUIRE(!result.empty());
  INFO(result);
  CHECK(result.find(" conflicts with an include directory of ") != std::string::npos);
}

TEST_CASE("target_model: targets with nested interface include directories are invalid", "[target_model]")
{
  target_model::Target_data liba_target_data;
  target_model::Target_data libb_target_data;
  liba_target_data.interface_include_directories = {"/some/include", "/some/other/include"};
  libb_target_data.interface_include_directories = {"/some/other/include/with/stuff"};

  std::map<target_model::Target, target_model::Target_data> target_to_target_data{
    {{"liba"}, liba_target_data},
    {{"libb"}, libb_target_data}};
  target_model::Target_model target_model{std::move(target_to_target_data)};

  const auto result = target_model.validate();
  REQUIRE(!result.empty());
  INFO(result);
  CHECK(result.find(" conflicts with an include directory of ") != std::string::npos);
}

TEST_CASE("target_model: targets with the same interface include directories with prefixes are invalid", "[target_model]")
{
  target_model::Target_data liba_target_data;
  target_model::Target_data libb_target_data;
  liba_target_data.interface_include_directories = {"/some/include", "/some/other/include"};
  libb_target_data.interface_include_directories = {"/some/other/include/with"};
  liba_target_data.interface_include_prefixes = {"with/stuff"};
  libb_target_data.interface_include_prefixes = {"stuff"};

  std::map<target_model::Target, target_model::Target_data> target_to_target_data{
    {{"liba"}, liba_target_data},
    {{"libb"}, libb_target_data}};
  target_model::Target_model target_model{std::move(target_to_target_data)};

  const auto result = target_model.validate();
  REQUIRE(!result.empty());
  INFO(result);
  CHECK(result.find(" conflicts with an include directory of ") != std::string::npos);
}

TEST_CASE("target_model: targets with the same interface include directory and only one prefix are invalid", "[target_model]")
{
  target_model::Target_data liba_target_data;
  target_model::Target_data libb_target_data;
  liba_target_data.interface_include_directories = {"/some/include", "/some/other/include"};
  libb_target_data.interface_include_directories = {"/some/other/include/"};
  liba_target_data.interface_include_prefixes = {"with"};

  std::map<target_model::Target, target_model::Target_data> target_to_target_data{
    {{"liba"}, liba_target_data},
    {{"libb"}, libb_target_data}};
  target_model::Target_model target_model{std::move(target_to_target_data)};

  const auto result = target_model.validate();
  REQUIRE(!result.empty());
  INFO(result);
  CHECK(result.find(" conflicts with an include directory of ") != std::string::npos);
}

TEST_CASE("target_model: targets with nested interface include directories with prefixes are invalid", "[target_model]")
{
  target_model::Target_data liba_target_data;
  target_model::Target_data libb_target_data;
  liba_target_data.interface_include_directories = {"/some/include", "/some/other/include"};
  libb_target_data.interface_include_directories = {"/some/other/include/"};
  liba_target_data.interface_include_prefixes = {"with"};
  libb_target_data.interface_include_prefixes = {"with/stuff"};

  std::map<target_model::Target, target_model::Target_data> target_to_target_data{
    {{"liba"}, liba_target_data},
    {{"libb"}, libb_target_data}};
  target_model::Target_model target_model{std::move(target_to_target_data)};

  const auto result = target_model.validate();
  REQUIRE(!result.empty());
  INFO(result);
  CHECK(result.find(" conflicts with an include directory of ") != std::string::npos);
}

TEST_CASE("target_model: targets with disjoint interface include directories with prefixes are valid", "[target_model]")
{
  target_model::Target_data liba_target_data;
  target_model::Target_data libb_target_data;
  liba_target_data.interface_include_directories = {"/some/include", "/some/other/include"};
  libb_target_data.interface_include_directories = {"/some/other/include"};
  liba_target_data.interface_include_prefixes = {"a_prefix"};
  libb_target_data.interface_include_prefixes = {"b_prefix"};

  std::map<target_model::Target, target_model::Target_data> target_to_target_data{
    {{"liba"}, liba_target_data},
    {{"libb"}, libb_target_data}};
  target_model::Target_model target_model{std::move(target_to_target_data)};

  const auto result = target_model.validate();
  INFO(result);
  REQUIRE(result.empty());
}
