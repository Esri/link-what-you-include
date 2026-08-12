// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <target_model/target_data.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("target_model: Target_data free functions", "[target_model]")
{
  target_model::Target_data target_data;
  target_data.interface_headers = {"/liba/include/a.h", "/liba/include/b.h"};
  target_data.interface_include_directories = {"/some/other/include", "/yet/another/include"};
  target_data.sources = {"/liba/src/a.cpp", "/liba/src/b.cpp"};

  SECTION("explicit interface headers")
  {
    CHECK(target_model::is_interface_header(target_data, "/liba/include/a.h"));
    CHECK(target_model::is_interface_header(target_data, "/liba/include/b.h"));
    CHECK(!target_model::is_interface_header(target_data, "/liba/include/c.h"));
  }
  SECTION("implicit interface headers")
  {
    CHECK(target_model::is_interface_header(target_data, "/some/other/include/liba/a.h"));
    CHECK(target_model::is_interface_header(target_data,
                                            "/some/other/include/other_prifix/a.h"));
    CHECK(target_model::is_interface_header(target_data, "/yet/another/include/liba/a.h"));
    CHECK(target_model::is_interface_header(target_data,
                                            "/yet/another/include/other_prefix/a.h"));
    CHECK(!target_model::is_interface_header(target_data, "/not/an/include/liba/c.h"));
  }
  SECTION("implicit interface headers with prefixes")
  {
    target_data.interface_include_prefixes = {"liba", "some_prefix"};

    CHECK(target_model::is_interface_header(target_data, "/some/other/include/liba/a.h"));
    CHECK(target_model::is_interface_header(target_data,
                                            "/some/other/include/some_prefix/a.h"));
    CHECK(!target_model::is_interface_header(target_data,
                                             "/some/other/include/other_prefix/a.h"));
    CHECK(target_model::is_interface_header(target_data, "/yet/another/include/liba/a.h"));
    CHECK(target_model::is_interface_header(target_data,
                                            "/yet/another/include/some_prefix/a.h"));
    CHECK(!target_model::is_interface_header(target_data,
                                             "/yet/another/include/other_prefix/a.h"));
    CHECK(!target_model::is_interface_header(target_data, "/not/an/include/liba/c.h"));
  }
  SECTION("explicit private sources")
  {
    CHECK(target_model::is_private_source(target_data, "/liba/src/a.cpp"));
    CHECK(target_model::is_private_source(target_data, "/liba/src/b.cpp"));
    CHECK(!target_model::is_private_source(target_data, "/liba/src/c.cpp"));
  }
  SECTION("explicit private headers")
  {
    target_data.headers.insert("/liba/src/a.hpp");
    CHECK(target_model::is_private_source(target_data, "/liba/src/a.hpp"));
    CHECK(!target_model::is_private_source(target_data, "/liba/src/b.hpp"));
  }
  SECTION("explicit private headers as sources")
  {
    target_data.sources.insert("/liba/src/a.hpp");
    CHECK(target_model::is_private_source(target_data, "/liba/src/a.hpp"));
    CHECK(!target_model::is_private_source(target_data, "/liba/src/b.hpp"));
  }
}
