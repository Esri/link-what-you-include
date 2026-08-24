// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <lwyi/dependency_scope.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("lwyi: Dependency_scope operator!", "[lwyi]")
{
  using namespace lwyi;
  CHECK(!Dependency_scope::none);
  CHECK(!!Dependency_scope::private_scope);
  CHECK(!!Dependency_scope::interface_scope);
  CHECK(!!Dependency_scope::public_scope);
}

TEST_CASE("lwyi: Dependency_scope operator|=", "[lwyi]")
{
  using namespace lwyi;

  const auto none = Dependency_scope::none;
  const auto prvt = Dependency_scope::private_scope;
  const auto ntfc = Dependency_scope::interface_scope;
  const auto pblc = Dependency_scope::public_scope;

  auto a = none;
  a |= none;
  CHECK(a == Dependency_scope::none);
  a = none;
  a |= prvt;
  CHECK(a == Dependency_scope::private_scope);
  a = none;
  a |= ntfc;
  CHECK(a == Dependency_scope::interface_scope);
  a = none;
  a |= pblc;
  CHECK(a == Dependency_scope::public_scope);

  a = prvt;
  a |= none;
  CHECK(a == Dependency_scope::private_scope);
  a = prvt;
  a |= prvt;
  CHECK(a == Dependency_scope::private_scope);
  a = prvt;
  a |= ntfc;
  CHECK(a == Dependency_scope::public_scope);
  a = prvt;
  a |= pblc;
  CHECK(a == Dependency_scope::public_scope);

  a = ntfc;
  a |= none;
  CHECK(a == Dependency_scope::interface_scope);
  a = ntfc;
  a |= prvt;
  CHECK(a == Dependency_scope::public_scope);
  a = ntfc;
  a |= ntfc;
  CHECK(a == Dependency_scope::interface_scope);
  a = ntfc;
  a |= pblc;
  CHECK(a == Dependency_scope::public_scope);

  a = pblc;
  a |= none;
  CHECK(a == Dependency_scope::public_scope);
  a = pblc;
  a |= prvt;
  CHECK(a == Dependency_scope::public_scope);
  a = pblc;
  a |= ntfc;
  CHECK(a == Dependency_scope::public_scope);
  a = pblc;
  a |= pblc;
  CHECK(a == Dependency_scope::public_scope);
}

TEST_CASE("lwyi: Dependency_scope operator&", "[lwyi]")
{
  using namespace lwyi;

  const auto none = Dependency_scope::none;
  const auto prvt = Dependency_scope::private_scope;
  const auto ntfc = Dependency_scope::interface_scope;
  const auto pblc = Dependency_scope::public_scope;

  auto a = none & none; // NOLINT(misc-redundant-expression)
  CHECK(a == Dependency_scope::none);
  a = none & prvt;
  CHECK(a == Dependency_scope::none);
  a = none & ntfc;
  CHECK(a == Dependency_scope::none);
  a = none & pblc;
  CHECK(a == Dependency_scope::none);

  a = prvt & none;
  CHECK(a == Dependency_scope::none);
  a = prvt & prvt; // NOLINT(misc-redundant-expression)
  CHECK(a == Dependency_scope::private_scope);
  a = prvt & ntfc;
  CHECK(a == Dependency_scope::none);
  a = prvt & pblc;
  CHECK(a == Dependency_scope::private_scope);

  a = ntfc & none;
  CHECK(a == Dependency_scope::none);
  a = ntfc & prvt;
  CHECK(a == Dependency_scope::none);
  a = ntfc & ntfc; // NOLINT(misc-redundant-expression)
  CHECK(a == Dependency_scope::interface_scope);
  a = ntfc & pblc;
  CHECK(a == Dependency_scope::interface_scope);

  a = pblc & none;
  CHECK(a == Dependency_scope::none);
  a = pblc & prvt;
  CHECK(a == Dependency_scope::private_scope);
  a = pblc & ntfc;
  CHECK(a == Dependency_scope::interface_scope);
  a = pblc & pblc; // NOLINT(misc-redundant-expression)
  CHECK(a == Dependency_scope::public_scope);
}

TEST_CASE("lwyi: Dependency_scope operator~", "[lwyi]")
{
  using namespace lwyi;
  CHECK((~Dependency_scope::none) == Dependency_scope::public_scope);
  CHECK((~Dependency_scope::private_scope) == Dependency_scope::interface_scope);
  CHECK((~Dependency_scope::interface_scope) == Dependency_scope::private_scope);
  CHECK((~Dependency_scope::public_scope) == Dependency_scope::none);
}
