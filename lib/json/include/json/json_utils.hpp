// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <simdjson.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <flat_map>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace json
{
enum class Type : uint8_t
{
  none = 0,
  boolean = 1U << 0U,
  integer = 1U << 1U,
  number = 1U << 2U,
  string = 1U << 3U,
  array = 1U << 4U,
  object = 1U << 5U,
};

constexpr Type operator|(Type lhs, Type rhs)
{
  return static_cast<Type>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

constexpr bool overlaps(Type lhs, Type rhs)
{
  return (std::to_underlying(lhs) & std::to_underlying(rhs)) != 0;
}

namespace detail
{
inline std::string_view describe_error(simdjson::error_code error)
{
  static constexpr std::array<std::pair<simdjson::error_code, std::string_view>, 18> messages{{
    {simdjson::TAPE_ERROR, "invalid JSON structure"},
    {simdjson::DEPTH_ERROR, "JSON nesting is too deep"},
    {simdjson::STRING_ERROR, "invalid JSON string"},
    {simdjson::T_ATOM_ERROR, "invalid \"true\" literal"},
    {simdjson::F_ATOM_ERROR, "invalid \"false\" literal"},
    {simdjson::N_ATOM_ERROR, "invalid \"null\" literal"},
    {simdjson::NUMBER_ERROR, "invalid JSON number"},
    {simdjson::BIGINT_ERROR, "JSON integer is too large"},
    {simdjson::UTF8_ERROR, "invalid UTF-8"},
    {simdjson::EMPTY, "empty JSON document"},
    {simdjson::UNESCAPED_CHARS, "unescaped character in JSON string"},
    {simdjson::UNCLOSED_STRING, "unterminated JSON string"},
    {simdjson::NUMBER_OUT_OF_RANGE, "JSON number is out of range"},
    {simdjson::INCOMPLETE_ARRAY_OR_OBJECT, "incomplete JSON document"},
    {simdjson::TRAILING_CONTENT, "unexpected content after JSON value"},
    {simdjson::CAPACITY, "JSON document is too large"},
    {simdjson::MEMALLOC, "out of memory while parsing JSON"},
    {simdjson::IO_ERROR, "unable to read file"}
  }};
  const auto found = std::ranges::find(messages, error, &decltype(messages)::value_type::first);
  return found != messages.end() ? found->second : "invalid JSON";
}

inline std::string load_error(const std::filesystem::path& path, simdjson::error_code error)
{
  return std::format("Failed to read JSON file {}: {}", path.string(), describe_error(error));
}

inline Type type_of(simdjson::dom::element element)
{
  switch (element.type())
  {
    case simdjson::dom::element_type::BOOL:
      return Type::boolean;
    case simdjson::dom::element_type::INT64:
    case simdjson::dom::element_type::UINT64:
      return Type::integer | Type::number;
    case simdjson::dom::element_type::DOUBLE:
      return Type::number;
    case simdjson::dom::element_type::STRING:
      return Type::string;
    case simdjson::dom::element_type::ARRAY:
      return Type::array;
    case simdjson::dom::element_type::OBJECT:
      return Type::object;
    default:
      return Type::none;
  }
}
} // namespace detail


struct Error
{
  Error(std::string text) // NOLINT(*-explicit-constructor)
  : message(std::move(text))
  {
  }
  Error(const char* text) // NOLINT(*-explicit-constructor)
  : message(text)
  {
  }

  std::string message;
  bool located{false};
};

template <typename T>
using Result = std::expected<T, Error>;

template <typename R>
concept Reader = requires(const R& reader, simdjson::dom::element element) {
  typename R::value_type;
  { R::accepts } -> std::convertible_to<Type>;
  { reader.read(element) } -> std::same_as<Result<typename R::value_type>>;
};

// JSON scalars that value<T>() can read directly
template <typename T>
concept Scalar = std::same_as<T, std::string> || std::same_as<T, bool> ||
                 std::integral<T> || std::floating_point<T>;

// Types that a JSON string (or object key) can be converted into by value.
template <typename T>
concept From_string = std::constructible_from<T, std::string> &&
                      !std::same_as<std::remove_cvref_t<T>, std::string_view>;

namespace detail
{
// "expected boolean or array"
inline std::string expected(Type types)
{
  static constexpr std::array<std::pair<Type, std::string_view>, 6> names{{
    {Type::boolean, "boolean"},
    {Type::integer, "integer"},
    {Type::number, "number"},
    {Type::string, "string"},
    {Type::array, "array"},
    {Type::object, "object"},
  }};
  std::string result = "expected";
  std::string_view separator = " ";
  for (const auto& [type, name] : names)
  {
    if (overlaps(types, type))
    {
      result += separator;
      result += name;
      separator = " or ";
    }
  }
  return result;
}

// Prefixes `context` onto an error unless it is already located.
inline Error within(Error error, std::string_view context)
{
  if (!error.located)
  {
    error.message = std::format("{}: {}", context, error.message);
  }
  return error;
}

template <Reader R>
Result<typename R::value_type> read(const R& reader, simdjson::dom::element element)
{
  if (!overlaps(type_of(element), R::accepts))
  {
    return std::unexpected(expected(R::accepts));
  }
  return reader.read(element);
}

template <typename T>
concept Is_expected = requires {
  typename T::value_type;
  typename T::error_type;
} && std::same_as<T, std::expected<typename T::value_type, typename T::error_type>>;
} // namespace detail


// ----------------------
// Scalars
// ----------------------

template <Scalar T>
struct Value_reader
{
  using value_type = T;
  static constexpr Type accepts = std::same_as<T, std::string> ? Type::string
                                  : std::same_as<T, bool>      ? Type::boolean
                                  : std::integral<T>           ? Type::integer
                                                               : Type::number;

  [[nodiscard]] static Result<T> read(simdjson::dom::element element)
  {
    if constexpr (std::same_as<T, std::string>)
    {
      return std::string(element.get_string().value_unsafe());
    }
    else if constexpr (std::same_as<T, bool>)
    {
      return element.get_bool().value_unsafe();
    }
    else if constexpr (std::integral<T>)
    {
      using Wide = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;
      Wide wide{};
      if (element.get(wide) || !std::in_range<T>(wide))
      {
        return std::unexpected("integer out of range");
      }
      return static_cast<T>(wide);
    }
    else
    {
      const double number = element.get_double().value_unsafe();
      if (number < std::numeric_limits<T>::lowest() || number > std::numeric_limits<T>::max())
      {
        return std::unexpected("number out of range");
      }
      return static_cast<T>(number);
    }
  }
};

template <Scalar T>
constexpr Value_reader<T> value()
{
  return {};
}

// ----------------------
// Conversions
// ----------------------

template <Reader Inner, typename Convert>
  requires std::invocable<const Convert&, typename Inner::value_type>
struct Transform_reader
{
  using converted_type =
    std::remove_cvref_t<std::invoke_result_t<const Convert&, typename Inner::value_type>>;
  using value_type = typename std::conditional_t<detail::Is_expected<converted_type>,
                                                 converted_type,
                                                 Result<converted_type>>::value_type;
  static_assert(!std::same_as<value_type, std::string_view>,
                "Conversions must return owning values");
  static constexpr Type accepts = Inner::accepts;

  Inner reader;
  Convert convert;

  [[nodiscard]] Result<value_type> read(simdjson::dom::element element) const
  {
    return reader.read(element).and_then(
      [this](typename Inner::value_type&& inner) -> Result<value_type>
      {
        return std::invoke(convert, std::move(inner));
      });
  }
};

template <Reader Inner, typename Convert>
constexpr auto transform(Inner reader, Convert convert)
{
  return Transform_reader<Inner, Convert>{std::move(reader), std::move(convert)};
}

template <Reader Inner, typename Check>
  requires std::same_as<std::invoke_result_t<const Check&, const typename Inner::value_type&>,
                        std::expected<void, std::string>>
constexpr auto validate(Inner reader, Check check)
{
  using Value = typename Inner::value_type;
  return transform(std::move(reader),
                   [check = std::move(check)](Value value) -> Result<Value>
                   {
                     return std::invoke(check, std::as_const(value))
                       .transform([&] { return std::move(value); });
                   });
}

// Reads a JSON string into a type constructible from std::string.
template <From_string T>
constexpr auto string_as()
{
  return transform(value<std::string>(),
                   [](std::string text)
                   {
                     return T{std::move(text)};
                   });
}

// ----------------------
// Collections
// ----------------------

template <typename Container, Reader Item>
struct Array_reader
{
  using value_type = Container;
  static constexpr Type accepts = Type::array;
  Item reader;

  [[nodiscard]] Result<Container> read(simdjson::dom::element element) const
  {
    Container result;
    std::size_t index = 0;
    for (auto item : element.get_array().value_unsafe())
    {
      auto parsed = detail::read(reader, item);
      if (!parsed)
      {
        return std::unexpected(
          detail::within(std::move(parsed.error()), std::format("element {}", index)));
      }
      result.insert(result.end(), std::move(*parsed));
      ++index;
    }
    return result;
  }
};

// JSON array -> std::vector, preserving order and duplicates.
template <Reader Item>
constexpr auto array(Item reader)
{
  return Array_reader<std::vector<typename Item::value_type>, Item>{std::move(reader)};
}

// JSON array -> std::set, ordering and deduplicating elements.
template <Reader Item>
constexpr auto set(Item reader)
{
  return Array_reader<std::set<typename Item::value_type>, Item>{std::move(reader)};
}

// `Container` is any map type with `try_emplace`, e.g. std::map or std::flat_map.
template <typename Container, Reader Mapped>
  requires From_string<typename Container::key_type> &&
           std::same_as<typename Container::mapped_type, typename Mapped::value_type>
struct Map_reader
{
  using value_type = Container;
  using Key = typename Container::key_type;
  static constexpr Type accepts = Type::object;
  Mapped reader;

  [[nodiscard]] Result<value_type> read(simdjson::dom::element element) const
  {
    value_type result;
    for (auto [key, item] : element.get_object().value_unsafe())
    {
      auto parsed = detail::read(reader, item);
      if (!parsed)
      {
        auto error = detail::within(std::move(parsed.error()), std::format("\"{}\"", key));
        error.located = true;
        return std::unexpected(std::move(error));
      }
      if (!result.try_emplace(Key{std::string(key)}, std::move(*parsed)).second)
      {
        return std::unexpected(std::format("duplicate key \"{}\"", key));
      }
    }
    return result;
  }
};

// JSON object with arbitrary keys -> std::map<Key, T>.
template <From_string Key = std::string, Reader Mapped>
constexpr auto map(Mapped reader)
{
  return Map_reader<std::map<Key, typename Mapped::value_type>, Mapped>{std::move(reader)};
}

// JSON object with arbitrary keys -> std::flat_map<Key, T>.
template <From_string Key = std::string, Reader Mapped>
constexpr auto flat_map(Mapped reader)
{
  return Map_reader<std::flat_map<Key, typename Mapped::value_type>, Mapped>{
    std::move(reader)};
}

// ----------------------
// Variants
// ----------------------

template <Reader... Readers>
  requires(sizeof...(Readers) > 0)
struct Variant_reader
{
  using value_type = std::variant<typename Readers::value_type...>;
  static constexpr Type accepts = (Readers::accepts | ...);
  std::tuple<Readers...> readers;

  [[nodiscard]] Result<value_type> read(simdjson::dom::element element) const
  {
    return read_from<0>(element);
  }

private:
  template <std::size_t Index>
  [[nodiscard]] Result<value_type> read_from(simdjson::dom::element element) const
  {
    const auto& reader = std::get<Index>(readers);
    if constexpr (Index + 1 < sizeof...(Readers))
    {
      if (!overlaps(detail::type_of(element), reader.accepts))
      {
        return read_from<Index + 1>(element);
      }
    }
    return reader.read(element).transform(
      [](auto&& parsed)
      {
        return value_type{std::in_place_index<Index>, std::forward<decltype(parsed)>(parsed)};
      });
  }
};

template <Reader... Readers>
constexpr auto variant(Readers... readers)
{
  return Variant_reader<Readers...>{std::tuple{std::move(readers)...}};
}

template <typename T, Reader... Readers>
  requires(std::constructible_from<T, typename Readers::value_type> && ...)
constexpr auto variant_as(Readers... readers)
{
  return transform(variant(std::move(readers)...),
                   [](typename Variant_reader<Readers...>::value_type parsed)
                   {
                     return std::visit(
                       [](auto&& alternative)
                       {
                         return T(std::forward<decltype(alternative)>(alternative));
                       },
                       std::move(parsed));
                   });
}

// ----------------------
// Objects
// ----------------------

template <typename Owner>
struct Field
{
  std::string name;
  bool required{false};
  std::function<Result<void>(Owner&, simdjson::dom::element)> assign;
};

namespace detail
{
template <typename Owner, typename Member, Reader R>
Field<Owner> field(std::string name, Member Owner::* member, R reader, bool required)
{
  static_assert(std::same_as<Member, typename R::value_type>,
                "The field reader must produce the member's type");
  return {std::move(name),
          required,
          [member, reader = std::move(reader)](Owner& owner, simdjson::dom::element element)
          {
            return read(reader, element).transform(
              [&](typename R::value_type&& parsed) { owner.*member = std::move(parsed); });
          }};
}
} // namespace detail

template <typename Owner, typename Member, Reader R = Value_reader<Member>>
Field<Owner> required(std::string name, Member Owner::* member, R reader = {})
{
  return detail::field(std::move(name), member, std::move(reader), true);
}

template <typename Owner, typename Member, Reader R = Value_reader<Member>>
Field<Owner> optional(std::string name, Member Owner::* member, R reader = {})
{
  return detail::field(std::move(name), member, std::move(reader), false);
}

template <std::default_initializable T>
struct Object_reader
{
  using value_type = T;
  static constexpr Type accepts = Type::object;
  std::vector<Field<T>> fields;
  bool allow_extra_fields{false};

  [[nodiscard]] Result<T> read(simdjson::dom::element element) const
  {
    T result{};
    std::set<std::string_view> seen;
    for (auto [key, item] : element.get_object().value_unsafe())
    {
      if (!seen.insert(key).second)
      {
        return std::unexpected(std::format("duplicate field \"{}\"", key));
      }
      const auto field = std::ranges::find(fields, key, &Field<T>::name);
      if (field == fields.end())
      {
        if (allow_extra_fields)
        {
          continue;
        }
        return std::unexpected(std::format("unexpected field \"{}\"", key));
      }
      if (auto status = field->assign(result, item); !status)
      {
        return std::unexpected(
          detail::within(std::move(status.error()), std::format("field \"{}\"", key)));
      }
    }
    for (const auto& field : fields)
    {
      if (field.required && !seen.contains(field.name))
      {
        return std::unexpected(std::format("missing required field \"{}\"", field.name));
      }
    }
    return result;
  }
};

namespace detail
{
template <typename T>
Object_reader<T> object(std::vector<Field<T>> fields, bool allow_extra_fields)
{
#ifndef NDEBUG
  std::set<std::string_view> names;
  for (const auto& field : fields)
  {
    assert(names.insert(field.name).second && "Field names must be unique");
  }
#endif
  return {std::move(fields), allow_extra_fields};
}
} // namespace detail

// A JSON object with exactly these fields; any other key is an error.
template <typename T, typename... Owners>
Object_reader<T> object(Field<Owners>... fields)
{
  static_assert((std::same_as<T, Owners> && ...), "Fields must be members of the object type");
  return detail::object<T>({std::move(fields)...}, false);
}

// A JSON object read through these fields; any other key is ignored.
template <typename T, typename... Owners>
Object_reader<T> partial_object(Field<Owners>... fields)
{
  static_assert((std::same_as<T, Owners> && ...), "Fields must be members of the object type");
  return detail::object<T>({std::move(fields)...}, true);
}

// ----------------------
// Entry points
// ----------------------

namespace detail
{
// Applies `reader` to a parsed document, or reports why parsing failed.
template <Reader R>
std::expected<typename R::value_type, std::string> read_parsed(
  simdjson::simdjson_result<simdjson::dom::element> parsed, const R& reader)
{
  simdjson::dom::element root;
  if (auto error = std::move(parsed).get(root))
  {
    return std::unexpected(std::string(detail::describe_error(error)));
  }
  return detail::read(reader, root).transform_error(
    [](Error error) { return std::move(error.message); });
}
} // namespace detail

// Parses `raw_json` and applies `reader`
template <Reader R>
std::expected<typename R::value_type, std::string> read_root(std::string_view raw_json,
                                                             const R& reader)
{
  simdjson::dom::parser parser;
  return detail::read_parsed(parser.parse(raw_json.data(), raw_json.size()), reader);
}

// Loads, parses, and reads a whole file.
template <Reader R>
std::expected<typename R::value_type, std::string> read_file(const std::filesystem::path& path,
                                                             const R& reader)
{
  simdjson::padded_string text;
  if (auto error = simdjson::padded_string::load(path.string()).get(text))
  {
    return std::unexpected(detail::load_error(path, error));
  }
  simdjson::dom::parser parser;
  return detail::read_parsed(parser.parse(text), reader)
    .transform_error([&](const std::string& error)
                     { return std::format("{}: {}", path.string(), error); });
}

// Loads a whole file as text.
inline std::expected<std::string, std::string> load_file(const std::filesystem::path& path)
{
  simdjson::padded_string text;
  if (auto error = simdjson::padded_string::load(path.string()).get(text))
  {
    return std::unexpected(detail::load_error(path, error));
  }
  return std::string(text.data(), text.length());
}
} // namespace json
