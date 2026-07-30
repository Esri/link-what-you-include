# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

# The link-what-you-include tool scans source files for `#include`'s and maps them to the libraries
# they are associated with. This works best when the dependent target has the INTERFACE_HEADER_SETS
# properties defined but this is not required.

# This package enables the generation of the compilation database and file API codemodel reply
# necessary for the link-what-you-include tool to perform validation

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_file_api(QUERY API_VERSION 1 CODEMODEL 2)
