// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cli/parse_arguments.hpp>
#include <message/message.hpp>
#include <src/run_lwyi.hpp>

#include <expected>
#include <string>

std::expected<int, std::string> print_error(const std::string& error)
{
  message::print(error, message::Style::error);
  return 1;
}

int main(int argc, const char* argv[])
{
  auto options = cli::parse_arguments(argc, argv);
  if (options.has_value())
  {
    message::configure({options->color_output, options->message_level});
    return run_lwyi(*options).or_else(print_error).value_or(1);
  }

  return print_error(options.error()).value_or(1);
}
