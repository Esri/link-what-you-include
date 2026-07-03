// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace util
{
namespace detail
{
template <class Toptions>
struct Terminal_arg
{
  std::vector<std::string_view> Toptions::* pmem;
};

template <class Toptions, typename... T>
class Arg_parser
{
public:
  constexpr explicit Arg_parser(std::tuple<std::pair<std::string_view, T>...> pairs)
  : pairs_(std::move(pairs))
  {
  }

  template <typename U>
  constexpr auto arg(std::string_view name, U Toptions::* pmem)
  {
    return Arg_parser<Toptions, T..., U Toptions::*>{
      std::tuple_cat(pairs_,
                     std::make_tuple(std::pair<std::string_view, U Toptions::*>{name, pmem}))};
  }

  template <typename U>
  constexpr auto arg(std::string_view name1, std::string_view name2, U Toptions::* pmem)
  {
    return Arg_parser<Toptions, T..., U Toptions::*, U Toptions::*>{std::tuple_cat(
      pairs_,
      std::make_tuple(std::pair<std::string_view, U Toptions::*>{name1, pmem},
                      std::pair<std::string_view, U Toptions::*>{name2, pmem}))};
  }

  constexpr auto terminal_arg(std::string_view name,
                              std::vector<std::string_view> Toptions::* pmem)
  {
    return Arg_parser<Toptions, T..., Terminal_arg<Toptions>>{
      std::tuple_cat(pairs_,
                     std::make_tuple(std::pair<std::string_view, Terminal_arg<Toptions>>{
                       name,
                       Terminal_arg<Toptions>{pmem}}))};
  }

  template <typename Tit>
  [[nodiscard]] std::expected<Toptions, std::string> parse(Tit begin, Tit end) const
  {
    Toptions options;
    auto error_string =
      parse_impl_(options, begin, end, std::make_index_sequence<sizeof...(T)>{});
    if (!error_string.empty())
    {
      return std::unexpected(std::move(error_string));
    }

    return options;
  }

private:
  template <typename Tit, size_t... Is>
  std::string parse_impl_(Toptions& options, Tit begin, Tit end, std::index_sequence<Is...>) const
  {
    while (begin != end)
    {
      std::string error_string;
      auto next = begin;
      const bool no_progress = ((next = try_parse_(options,
                                                   std::get<Is>(pairs_).first,
                                                   std::get<Is>(pairs_).second,
                                                   begin,
                                                   end,
                                                   error_string),
                                 next == begin) &&
                                ...);
      if (no_progress)
      {
        return std::format("unrecognized option: {}", *begin);
      }
      if (!error_string.empty())
      {
        return error_string;
      }
      begin = next;
    }
    return {};
  }

  template <typename Tit, typename U>
  Tit try_parse_(Toptions& options,
                 std::string_view name,
                 U value,
                 Tit begin,
                 Tit end,
                 std::string& error_string) const
  {
    assert(begin != end);

    std::string_view arg{*begin};
    if (arg.empty())
    {
      error_string = "expect non-empty args";
      return begin;
    }

    const bool can_omit_space = 1 < arg.size() && arg[0] == '-' && arg[1] != '-';
    const auto arg_head = can_omit_space ? arg.substr(0, std::min(arg.size(), name.size()))
                                         : arg;
    const auto arg_tail = arg.substr(arg_head.size());
    if (arg_head == name)
    {
      if constexpr (std::is_same_v<U, bool Toptions::*>)
      {
        if (!arg_tail.empty())
        {
          error_string = std::format("argument {} does not expect a value", arg);
          return begin;
        }
        options.*value = true;
        return ++begin;
      }
      else
      {
        if (arg_tail.empty())
        {
          ++begin;
          if (begin == end)
          {
            error_string = std::format("argument {} expects a value", arg);
            return begin;
          }
        }

        if constexpr (std::is_same_v<U, std::string_view Toptions::*> ||
                      std::is_same_v<U, std::string Toptions::*>)
        {
          const auto param = arg_tail.empty() ? std::string_view(*begin) : arg_tail;
          assert(!param.empty());
          if (param[0] == '-')
          {
            error_string = std::format("argument {} expects a value, got {}.", arg, param);
            return begin;
          }

          options.*value = param;
          return ++begin;
        }
        else if constexpr (std::is_same_v<U, std::vector<std::string_view> Toptions::*> ||
                           std::is_same_v<U, std::vector<std::string> Toptions::*>)
        {
          if (!arg_tail.empty())
          {
            error_string =
              std::format("argument {} must have a space before the first value.", arg);
            return begin;
          }
          for (; begin != end; ++begin)
          {
            std::string_view param(*begin);
            assert(!param.empty());
            if (param[0] == '-')
            {
              if ((options.*value).empty())
              {
                error_string = std::format("argument {} expects one or more values.", arg);
              }
              return begin;
            }
            (options.*value).push_back(*begin);
          }

          return end;
        }
        else if constexpr (std::is_same_v<U, uint32_t Toptions::*>)
        {
          const auto param = arg_tail.empty() ? std::string_view(*begin) : arg_tail;
          assert(!param.empty());
          if (param[0] == '-')
          {
            error_string = std::format("argument {} expects a value, got {}.", arg, param);
            return begin;
          }

          std::stringstream ss(std::string{param});
          ss >> options.*value;

          if (ss.bad())
          {
            error_string =
              std::format("argument {} expects an integer value, got {}.", arg, param);
            return begin;
          }

          return ++begin;
        }
        else if constexpr (std::is_same_v<U, Terminal_arg<Toptions>>)
        {
          std::copy(begin, end, std::back_inserter(options.*(value.pmem)));
          return end;
        }
        else
        {
          // TODO: can this be static?
          assert(!"not yet supported");
        }
      }
    }

    return begin;
  }

  std::tuple<std::pair<std::string_view, T>...> pairs_;
};
} // namespace detail

template <typename Toptions>
constexpr detail::Arg_parser<Toptions> arg_parser()
{
  return detail::Arg_parser<Toptions>{std::make_tuple()};
}
} // namespace util
