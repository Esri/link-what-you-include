// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <json/json_utils.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <flat_map>
#include <format>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
struct Options
{
  std::string name{"default"};
  bool enabled{true};
  std::vector<int> numbers{42};
};

const auto options = json::object<Options>( // NOLINT(bugprone-throwing-static-initialization)
  json::required("name", &Options::name),
  json::optional("enabled", &Options::enabled),
  json::optional("numbers", &Options::numbers, json::array(json::value<int>())));
} // namespace

TEST_CASE("json: fields load in input order and preserve defaults", "[json]")
{
  auto result = json::read_root(R"({"numbers": [3, 1, 3], "enabled": false, "name": "test"})", options);
  REQUIRE(result);
  CHECK(result->name == "test");
  CHECK_FALSE(result->enabled);
  CHECK(result->numbers == std::vector<int>{3, 1, 3});

  result = json::read_root(R"({"name": "minimal"})", options);
  REQUIRE(result);
  CHECK(result->enabled);
  CHECK(result->numbers == std::vector<int>{42});

  result = json::read_root(R"({"name": "empty", "numbers": []})", options);
  REQUIRE(result);
  CHECK(result->numbers.empty());
}

TEST_CASE("json: missing and invalid fields identify their context", "[json]")
{
  auto result = json::read_root("{}", options);
  REQUIRE_FALSE(result);
  CHECK(result.error() == "missing required field \"name\"");

  result = json::read_root(R"({"name": "x", "enabled": null})", options);
  REQUIRE_FALSE(result);
  CHECK(result.error() == "field \"enabled\": expected boolean");

  result = json::read_root(R"({"name": "x", "numbers": [1, "wrong"]})", options);
  REQUIRE_FALSE(result);
  CHECK(result.error() == "field \"numbers\": element 1: expected integer");

  result = json::read_root("[]", options);
  REQUIRE_FALSE(result);
  CHECK(result.error() == "expected object");
}

TEST_CASE("json: an absent optional field is the same as its default value", "[json]")
{
  auto absent = json::read_root(R"({"name": "x"})", options);
  auto explicit_default = json::read_root(R"({"name": "x", "enabled": true, "numbers": [42]})", options);
  REQUIRE(absent);
  REQUIRE(explicit_default);
  CHECK(absent->enabled == explicit_default->enabled);
  CHECK(absent->numbers == explicit_default->numbers);

  // null is a value of the wrong type, not a way to spell "absent".
  auto null = json::read_root(R"({"name": "x", "enabled": null})", options);
  REQUIRE_FALSE(null);
  CHECK(null.error() == "field \"enabled\": expected boolean");
}

TEST_CASE("json: variant readers select by JSON type", "[json]")
{
  const auto schema = json::variant(json::value<bool>(), json::array(json::value<int>()));

  auto boolean = json::read_root("false", schema);
  REQUIRE(boolean);
  CHECK(std::holds_alternative<bool>(*boolean));
  CHECK_FALSE(std::get<bool>(*boolean));

  auto numbers = json::read_root("[1, 2]", schema);
  REQUIRE(numbers);
  CHECK(std::get<std::vector<int>>(*numbers) == std::vector<int>{1, 2});

  auto invalid_element = json::read_root(R"([1, "wrong"])", schema);
  REQUIRE_FALSE(invalid_element);
  CHECK(invalid_element.error() == "element 1: expected integer");

  auto invalid_type = json::read_root("null", schema);
  REQUIRE_FALSE(invalid_type);
  CHECK(invalid_type.error() == "expected boolean or array");
}

TEST_CASE("json: unknown fields are rejected", "[json]")
{
  const auto text = R"({"extra": {"nested": [1, false]}, "name": "x"})";
  auto rejected = json::read_root(text, options);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error() == "unexpected field \"extra\"");
  CHECK_FALSE(json::read_root(text, options));
}

TEST_CASE("json: the entire document is validated", "[json]")
{
  const std::vector<std::string_view> invalid{
    "", "{", R"({"name": "x",})", R"({"name": "x"} {})",
    R"({"name": "x"} trailing)", R"({"name": "x", "ignored": tru})",
    R"({"name": "x", "ignored": [1,]})", R"({"name": "x", "ignored": "\q"})"};
  for (auto text : invalid)
  {
    CAPTURE(text);
    auto result = json::read_root(text, options);
    REQUIRE_FALSE(result);
    CHECK_FALSE(result.error().empty());
    CHECK(result.error().find('\n') == std::string::npos);
    CHECK(result.error().find("_ERROR") == std::string::npos);
  }
  auto invalid_literal = json::read_root(R"({"name":"x","extra":tru})", options);
  REQUIRE_FALSE(invalid_literal);
  CHECK(invalid_literal.error() == "invalid \"true\" literal");
}

TEST_CASE("json: duplicate object fields and map keys are rejected", "[json]")
{
  auto repeated = json::read_root(R"({"name": "first", "\u006eame": "second"})", options);
  REQUIRE_FALSE(repeated);
  CHECK(repeated.error() == "duplicate field \"name\"");

  auto map = json::read_root(R"({"x": 1, "x": 2})", json::map(json::value<int>()));
  REQUIRE_FALSE(map);
  CHECK(map.error() == "duplicate key \"x\"");
}

TEST_CASE("json: flat_map reads the same objects as map", "[json]")
{
  auto flat = json::read_root(R"({"b": 2, "a": 1})", json::flat_map(json::value<int>()));
  REQUIRE(flat);
  CHECK(*flat == std::flat_map<std::string, int>{{"a", 1}, {"b", 2}});

  auto repeated = json::read_root(R"({"x": 1, "x": 2})", json::flat_map(json::value<int>()));
  REQUIRE_FALSE(repeated);
  CHECK(repeated.error() == "duplicate key \"x\"");

  auto wrong = json::read_root(R"({"x": "one"})", json::flat_map(json::value<int>()));
  REQUIRE_FALSE(wrong);
  CHECK(wrong.error() == "\"x\": expected integer");
}

TEST_CASE("json: schemas own nested schemas and field names", "[json]")
{
  struct Root
  {
    std::map<std::string, Options> entries;
  };
  auto make_schema = []
  {
    std::string name = "a dynamically allocated field name";
    auto child = json::object<Options>(json::required(name, &Options::name));
    return json::object<Root>(json::required("entries", &Root::entries, json::map(child)));
  };
  const auto schema = make_schema();
  auto result = json::read_root(
    R"({"entries": {"a": {"a dynamically allocated field name": "value"},
                     "b": {"a dynamically allocated field name": "another"}}})",
    schema);
  REQUIRE(result);
  CHECK(result->entries.at("a").name == "value");
  CHECK(result->entries.at("b").name == "another");
}

TEST_CASE("json: strings and map keys outlive the parser and input", "[json]")
{
  auto result = []
  {
    std::string text = R"({"long dynamically allocated key": ["escaped\nstring", "\u03bb"]})";
    return json::read_root(text, json::map(json::array(json::value<std::string>())));
  }();
  REQUIRE(result);
  CHECK(result->at("long dynamically allocated key") ==
        std::vector<std::string>{"escaped\nstring", "\xce\xbb"});
}

TEST_CASE("json: array and set readers compose with maps", "[json]")
{
  auto schema = json::map(json::array(json::set(json::value<int>())));
  auto result = json::read_root(R"({"a": [[3, 1, 3], []], "b": []})", schema);
  REQUIRE(result);
  CHECK(result->at("a") == std::vector<std::set<int>>{{1, 3}, {}});
  CHECK(result->at("b").empty());

  auto invalid = json::read_root(R"({"a/b~c": [[1], ["wrong"]]})", schema);
  REQUIRE_FALSE(invalid);
  CHECK(invalid.error() == "\"a/b~c\": element 1: element 0: expected integer");
}

TEST_CASE("json: integer readers check type sign and narrowing", "[json]")
{
  auto maximum = json::read_root("18446744073709551615", json::value<uint64_t>());
  REQUIRE(maximum);
  CHECK(*maximum == std::numeric_limits<uint64_t>::max());
  auto minimum = json::read_root("-9223372036854775808", json::value<int64_t>());
  REQUIRE(minimum);
  CHECK(*minimum == std::numeric_limits<int64_t>::min());
  CHECK_FALSE(json::read_root("18446744073709551615", json::value<int64_t>()));
  CHECK_FALSE(json::read_root("-1", json::value<unsigned>()));
  CHECK_FALSE(json::read_root("256", json::value<uint8_t>()));
  CHECK_FALSE(json::read_root("1.5", json::value<int>()));
  CHECK_FALSE(json::read_root("true", json::value<int>()));
  CHECK_FALSE(json::read_root(R"("12")", json::value<int>()));
  CHECK_FALSE(json::read_root("1e100", json::value<float>()));
  auto number = json::read_root("1.5", json::value<double>());
  REQUIRE(number);
  CHECK(*number == 1.5); // NOLINT(cppcoreguidelines-avoid-magic-numbers)
}

TEST_CASE("json: custom conversions can validate domain values", "[json]")
{
  auto nonempty = json::transform(json::value<std::string>(),
    [](std::string text) -> std::expected<std::string, std::string>
    {
      if (text.empty())
      {
        return std::unexpected("must not be empty");
      }
      return text;
    });
  auto result = json::read_root(R"(["valid", ""])", json::array(nonempty));
  REQUIRE_FALSE(result);
  CHECK(result.error() == "element 1: must not be empty");

  auto with_colon = json::transform(json::value<int>(),
    [](int) -> std::expected<int, std::string>
    {
      return std::unexpected("value: not allowed");
    });
  auto nested = json::read_root(R"({"a: b": [0]})", json::map(json::array(with_colon)));
  REQUIRE_FALSE(nested);
  CHECK(nested.error() == "\"a: b\": element 0: value: not allowed");

  struct Name
  {
    std::string text;
    auto operator<=>(const Name&) const = default;
  };
  auto names = json::read_root(R"({"key": ["name", "name"]})",
                               json::map<Name>(json::set(json::string_as<Name>())));
  REQUIRE(names);
  CHECK(names->at(Name{"key"}) == std::set<Name>{Name{"name"}});
}

TEST_CASE("json: an empty object schema is usable", "[json]")
{
  struct Empty {};
  auto schema = json::object<Empty>();
  CHECK(json::read_root("{}", schema).has_value());
  CHECK_FALSE(json::read_root(R"({"extra": 1})", schema));
}

TEST_CASE("json: file loading returns owning text and reports the failing path", "[json]")
{
  const auto path = std::filesystem::path(__FILE__).parent_path() / "sample.json";
  auto text = json::load_file(path);
  REQUIRE(text);
  auto result = json::read_root(*text, options);
  REQUIRE(result);
  CHECK(result->name == "sample");
  CHECK(result->numbers == std::vector<int>{1, 2});

  // A source file cannot be the parent directory of another file.
  const auto missing = std::filesystem::path(__FILE__) / "missing.json";
  auto error = json::load_file(missing);
  REQUIRE_FALSE(error);
  CHECK(error.error().find(missing.string()) != std::string::npos);
}

TEST_CASE("json: variant readers distinguish integers from fractional numbers", "[json]")
{
  const auto schema = json::variant(json::value<int>(), json::value<double>());

  auto integer = json::read_root("2", schema);
  REQUIRE(integer);
  CHECK(std::get<int>(*integer) == 2);

  auto fractional = json::read_root("2.5", schema);
  REQUIRE(fractional);
  CHECK(std::get<double>(*fractional) == 2.5); // NOLINT(cppcoreguidelines-avoid-magic-numbers)

  auto whole_double = json::read_root("2.0", schema);
  REQUIRE(whole_double);
  CHECK(std::holds_alternative<double>(*whole_double));

  auto out_of_range = json::read_root("4294967296", schema);
  REQUIRE_FALSE(out_of_range);
  CHECK(out_of_range.error() == "integer out of range");
}

TEST_CASE("json: variant alternatives may share a value type", "[json]")
{
  const auto schema = json::variant(
    json::value<std::string>(),
    json::transform(json::value<bool>(),
                    [](bool flag)
                    {
                      return std::string(flag ? "yes" : "no");
                    }));
  auto text = json::read_root(R"("maybe")", schema);
  REQUIRE(text);
  CHECK(text->index() == 0);
  CHECK(std::get<0>(*text) == "maybe");

  auto flag = json::read_root("true", schema);
  REQUIRE(flag);
  CHECK(flag->index() == 1);
  CHECK(std::get<1>(*flag) == "yes");
}

TEST_CASE("json: transformed variants describe every accepted type", "[json]")
{
  const auto schema = json::transform(
    json::variant(json::value<bool>(), json::array(json::value<int>())),
    [](const std::variant<bool, std::vector<int>>& parsed)
    {
      return parsed.index();
    });
  CHECK(json::read_root("[true, [1]]", json::array(schema)).value() == std::vector<std::size_t>{0, 1});

  auto rejected = json::read_root(R"({"key": "text"})", json::map(schema));
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error() == "\"key\": expected boolean or array");
}

TEST_CASE("json: variant_as constructs one type from any alternative", "[json]")
{
  struct Count
  {
    explicit Count(bool flag)
    : value(flag ? 1 : 0)
    {
    }
    explicit Count(const std::vector<int>& items)
    : value(static_cast<int>(items.size()))
    {
    }
    int value;
  };
  const auto schema =
    json::variant_as<Count>(json::value<bool>(), json::array(json::value<int>()));

  CHECK(json::read_root("true", schema)->value == 1);
  CHECK(json::read_root("[4, 5, 6]", schema)->value == 3);

  auto rejected = json::read_root(R"("text")", schema);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error() == "expected boolean or array");
}

TEST_CASE("json: validate checks fields and whole objects", "[json]")
{
  struct Range
  {
    int low{0};
    int high{0};
  };
  const int limit = 100;
  const auto bounded = json::validate(json::value<int>(),
    [limit](const int& number) -> std::expected<void, std::string>
    {
      if (number > limit)
      {
        return std::unexpected(std::format("must be at most {}", limit));
      }
      return {};
    });
  const auto schema = json::validate(
    json::object<Range>(json::required("low", &Range::low, bounded),
                        json::required("high", &Range::high, bounded)),
    [](const Range& range) -> std::expected<void, std::string>
    {
      if (range.low > range.high)
      {
        return std::unexpected("low must not exceed high");
      }
      return {};
    });

  auto valid = json::read_root(R"({"low": 1, "high": 2})", schema);
  REQUIRE(valid);
  CHECK(valid->low == 1);
  CHECK(valid->high == 2);

  auto field = json::read_root(R"([{"low": 1, "high": 200}])", json::array(schema));
  REQUIRE_FALSE(field);
  CHECK(field.error() == "element 0: field \"high\": must be at most 100");

  auto object = json::read_root(R"({"x": {"low": 3, "high": 2}})", json::map(schema));
  REQUIRE_FALSE(object);
  CHECK(object.error() == "\"x\": low must not exceed high");
}

TEST_CASE("json: errors are reported relative to the nearest map key", "[json]")
{
  struct Entry
  {
    std::vector<int> values;
  };
  struct Root
  {
    std::map<std::string, std::map<std::string, Entry>> groups;
  };
  const auto entry = json::validate(
    json::object<Entry>(json::optional("values", &Entry::values, json::array(json::value<int>()))),
    [](const Entry& checked) -> std::expected<void, std::string>
    {
      if (checked.values.empty())
      {
        return std::unexpected("values must not be empty");
      }
      return {};
    });
  const auto schema = json::object<Root>(
    json::required("groups", &Root::groups, json::map(json::map(entry))));

  // The path inside the entry is kept; the enclosing group and field are not.
  auto element = json::read_root(R"({"groups": {"g": {"e": {"values": [1, "x"]}}}})", schema);
  REQUIRE_FALSE(element);
  CHECK(element.error() == "\"e\": field \"values\": element 1: expected integer");

  auto check = json::read_root(R"({"groups": {"g": {"e": {}}}})", schema);
  REQUIRE_FALSE(check);
  CHECK(check.error() == "\"e\": values must not be empty");

  // Errors about a map itself are located by the context around it.
  auto duplicate = json::read_root(R"({"groups": {"g": {"e": {"values": [1]}, "e": {"values": [2]}}}})", schema);
  REQUIRE_FALSE(duplicate);
  CHECK(duplicate.error() == "\"g\": duplicate key \"e\"");

  auto wrong_type = json::read_root(R"({"groups": []})", schema);
  REQUIRE_FALSE(wrong_type);
  CHECK(wrong_type.error() == "field \"groups\": expected object");
}

TEST_CASE("json: partial objects ignore fields they do not list", "[json]")
{
  struct Named
  {
    std::string name;
  };
  const auto partial = json::partial_object<Named>(json::required("name", &Named::name));

  auto result = json::read_root(R"({"extra": {"nested": [1]}, "name": "x", "other": 2})", partial);
  REQUIRE(result);
  CHECK(result->name == "x");

  // Listed fields are still checked, and duplicates are still errors.
  auto wrong_type = json::read_root(R"({"name": 1, "extra": true})", partial);
  REQUIRE_FALSE(wrong_type);
  CHECK(wrong_type.error() == "field \"name\": expected string");
  CHECK_FALSE(json::read_root(R"({"name": "x", "extra": 1, "extra": 2})", partial));
  CHECK_FALSE(json::read_root(R"({"extra": 1})", partial));

  // object() is unchanged: extra fields are rejected.
  auto strict = json::read_root(R"({"name": "x", "extra": 1})",
                                json::object<Named>(json::required("name", &Named::name)));
  REQUIRE_FALSE(strict);
  CHECK(strict.error() == "unexpected field \"extra\"");
}

TEST_CASE("json: read_file names the file in every error", "[json]")
{
  const auto directory = std::filesystem::path(__FILE__).parent_path();
  auto result = json::read_file(directory / "sample.json", options);
  REQUIRE(result);
  CHECK(result->name == "sample");

  const auto missing = std::filesystem::path(__FILE__) / "missing.json";
  auto unreadable = json::read_file(missing, options);
  REQUIRE_FALSE(unreadable);
  CHECK(unreadable.error().starts_with("Failed to read JSON file " + missing.string()));

  // This source file is not JSON at all.
  auto malformed = json::read_file(std::filesystem::path(__FILE__), options);
  REQUIRE_FALSE(malformed);
  CHECK(malformed.error().starts_with(std::filesystem::path(__FILE__).string() + ": "));

  auto schema_error = json::read_file(
    directory / "sample.json",
    json::object<Options>(json::required("name", &Options::name)));
  REQUIRE_FALSE(schema_error);
  CHECK(schema_error.error().starts_with((directory / "sample.json").string() + ": unexpected field"));
}
