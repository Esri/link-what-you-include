// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <expected>
#include <string>

namespace cli
{
struct Command_options;
}

auto run_lwyi(const cli::Command_options& options) -> std::expected<int, std::string>;
