// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cli/command_options.hpp>

#include <expected>
#include <string>

namespace cli
{
auto parse_arguments(int argc, const char* const* argv)
  -> std::expected<Command_options, std::string>;
} // namespace lwyi
