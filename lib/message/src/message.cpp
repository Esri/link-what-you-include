// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <message/message.hpp>

#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace message
{
namespace
{
Output_options g_options{}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef _WIN32
auto enable_virtual_terminal_processing(HANDLE handle) -> bool
{
  if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
  {
    return false;
  }

  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode))
  {
    return false;
  }

  if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
  {
    return true;
  }

  return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

auto enable_windows_color() -> bool
{
  const auto stdout_ok =
    enable_virtual_terminal_processing(GetStdHandle(STD_OUTPUT_HANDLE));
  const auto stderr_ok =
    enable_virtual_terminal_processing(GetStdHandle(STD_ERROR_HANDLE));
  return stdout_ok || stderr_ok;
}
#endif

auto message_level() -> Message_level
{
  return g_options.message_level;
}
} // namespace

void configure(Output_options options)
{
#ifdef _WIN32
  if (options.color && !enable_windows_color())
  {
    options.color = false;
  }
#endif

  g_options = options;
}

auto paint(std::string_view text, Style style) -> std::string
{
  if (!g_options.color || style == Style::plain)
  {
    return std::string{text};
  }

  const char* code = "";
  switch (style)
  {
    case Style::plain:
      return std::string{text};
    case Style::heading:
      code = "1;36";
      break;
    case Style::success:
      code = "1;32";
      break;
    case Style::warning:
      code = "1;33";
      break;
    case Style::error:
      code = "1;31";
      break;
    case Style::note:
      code = "1;34";
      break;
  }

  return std::string{"\x1b["} + code + "m" + std::string{text} + "\x1b[0m";
}

auto verbose_enabled() -> bool
{
  return message_level() >= Message_level::verbose;
}

void print(std::string_view text, Style style)
{
  std::cout << paint(text, style) << '\n';
}

void blank_line()
{
  std::cout << '\n';
}

void heading(std::string_view text)
{
  print(text, Style::heading);
}

void status(std::string_view label, std::string_view text, Style style)
{
  std::cout << paint(label, style) << ": " << text << '\n';
}

void info(std::string_view text)
{
  status("info", text);
}

void error(std::string_view text)
{
  status("error", text, Style::error);
}

void error_block(std::string_view title, std::string_view details) // NOLINT(bugprone-easily-swappable-parameters)
{
  status("error", title, Style::error);
  if (!details.empty())
  {
    print(details);
  }
}

void note(std::string_view text)
{
  status("note", text, Style::note);
}

void warning(std::string_view text)
{
  status("warning", text, Style::warning);
}

auto debug_enabled() -> bool
{
  return message_level() >= Message_level::debug;
}

void debug(std::string_view text)
{
  if (debug_enabled())
  {
    status("debug", text, Style::warning);
  }
}
} // namespace message
