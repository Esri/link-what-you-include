// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

namespace message
{
enum class Message_level : uint8_t
{
  normal,
  verbose,
  debug,
};

enum class Style
{
  plain,
  heading,
  success,
  warning,
  error,
  note,
};

struct Output_options
{
  bool color{false};
  Message_level message_level{Message_level::normal};
};

void configure(Output_options options);
void print(std::string_view text, Style style = Style::plain);
void blank_line();
void heading(std::string_view text);
void status(std::string_view label, std::string_view text, Style style = Style::plain);
void info(std::string_view text);
void error(std::string_view text);
void error_block(std::string_view title, std::string_view details = {});
void note(std::string_view text);
void warning(std::string_view text);
void debug(std::string_view text);
bool verbose_enabled();

template <typename... TArgs>
void print(std::format_string<TArgs...> format, TArgs&&... args)
{
  print(std::format(format, std::forward<TArgs>(args)...));
}

template <typename... TArgs>
void heading(std::format_string<TArgs...> format, TArgs&&... args)
{
  heading(std::format(format, std::forward<TArgs>(args)...));
}

template <typename... TArgs>
void status(std::string_view label, std::format_string<TArgs...> format, TArgs&&... args)
{
  status(label, std::format(format, std::forward<TArgs>(args)...));
}

template <typename... TArgs>
void info(std::format_string<TArgs...> format, TArgs&&... args)
{
  info(std::format(format, std::forward<TArgs>(args)...));
}

template <typename... TArgs>
void error(std::format_string<TArgs...> format, TArgs&&... args)
{
  error(std::format(format, std::forward<TArgs>(args)...));
}

template <typename... TArgs>
void note(std::format_string<TArgs...> format, TArgs&&... args)
{
  note(std::format(format, std::forward<TArgs>(args)...));
}

template <typename... TArgs>
void warning(std::format_string<TArgs...> format, TArgs&&... args)
{
  warning(std::format(format, std::forward<TArgs>(args)...));
}

template <typename... TArgs>
void debug(std::format_string<TArgs...> format, TArgs&&... args)
{
  debug(std::format(format, std::forward<TArgs>(args)...));
}
} // namespace message
