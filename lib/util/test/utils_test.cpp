// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <util/utils.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("util: is_in_directory", "[util]")
{
  // absolute vs absolute
#if _WIN32
  CHECK(util::is_in_directory("C:/a/b/c", "C:/a/b/c/file.h"));
  CHECK(util::is_in_directory("C:/a/b/c", "C:/a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "C:/a/b/q/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "C:/a/b/q/d/e/file.h"));
#else
  CHECK(util::is_in_directory("/a/b/c", "/a/b/c/file.h"));
  CHECK(util::is_in_directory("/a/b/c", "/a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "/a/b/q/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "/a/b/q/d/e/file.h"));
#endif

  // relative vs relative
  CHECK(util::is_in_directory("a/b/c", "a/b/c/file.h"));
  CHECK(util::is_in_directory("a/b/c", "a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "a/b/q/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "a/b/q/d/e/file.h"));

  // absolute vs relative
#if _WIN32
  CHECK(!util::is_in_directory("C:/a/b/c", "a/b/c/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "a/b/q/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "a/b/q/d/e/file.h"));

  CHECK(!util::is_in_directory("C:/a/b/c", "/a/b/c/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "/a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "/a/b/q/file.h"));
  CHECK(!util::is_in_directory("C:/a/b/c", "/a/b/q/d/e/file.h"));
#else
  CHECK(!util::is_in_directory("/a/b/c", "a/b/c/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "a/b/q/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "a/b/q/d/e/file.h"));
#endif

  // relative vs absolute
#if _WIN32
  CHECK(!util::is_in_directory("a/b/c", "C:/a/b/c/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "C:/a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "C:/a/b/q/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "C:/a/b/q/d/e/file.h"));

  CHECK(!util::is_in_directory("/a/b/c", "C:/a/b/c/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "C:/a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "C:/a/b/q/file.h"));
  CHECK(!util::is_in_directory("/a/b/c", "C:/a/b/q/d/e/file.h"));
#else
  CHECK(!util::is_in_directory("a/b/c", "/a/b/c/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "/a/b/c/d/e/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "/a/b/q/file.h"));
  CHECK(!util::is_in_directory("a/b/c", "/a/b/q/d/e/file.h"));
#endif

  // directory is in itself
#if _WIN32
  CHECK(util::is_in_directory("C:/a/b/c", "C:/a/b/c"));
  CHECK(util::is_in_directory("C:/a/b/c/", "C:/a/b/c/"));
#else
  CHECK(util::is_in_directory("/a/b/c", "/a/b/c"));
  CHECK(util::is_in_directory("/a/b/c/", "/a/b/c/"));
#endif

  CHECK(util::is_in_directory("a/b/c", "a/b/c"));
  CHECK(util::is_in_directory("a/b/c/", "a/b/c/"));
}
