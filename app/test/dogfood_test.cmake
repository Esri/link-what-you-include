# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED LWYI_EXECUTABLE)
  message(FATAL_ERROR "LWYI_EXECUTABLE is required")
endif()

if(NOT DEFINED LWYI_BINARY_DIR)
  message(FATAL_ERROR "LWYI_BINARY_DIR is required")
endif()

if(NOT DEFINED LWYI_EXPECT_COLOR)
  message(FATAL_ERROR "LWYI_EXPECT_COLOR is required")
endif()

if(NOT DEFINED LWYI_EXPECT_MESSAGE_LEVEL)
  message(FATAL_ERROR "LWYI_EXPECT_MESSAGE_LEVEL is required")
endif()

set(common_regexes
  "Build System"
  "Loading metadata from .*link_what_you_include_info\\.json"
  "Scanning with [0-9]+ thread"
)
set(verbose_regexes
  "Processed [0-9]+ source files"
)
string(ASCII 27 ansi_escape)
set(color_regexes "${ansi_escape}\\[[0-9;]+m")
set(debug_regexes "debug:")

set(expect_verbose FALSE)
set(expect_debug FALSE)

set(command_args "${LWYI_EXECUTABLE}" -d "${LWYI_BINARY_DIR}")
if(LWYI_EXPECT_MESSAGE_LEVEL STREQUAL "debug")
  set(expect_verbose TRUE)
  set(expect_debug TRUE)
  list(INSERT command_args 1 --debug)
elseif(LWYI_EXPECT_MESSAGE_LEVEL STREQUAL "verbose")
  set(expect_verbose TRUE)
  list(INSERT command_args 1 --verbose)
endif()
if(LWYI_EXPECT_COLOR)
  list(INSERT command_args 1 --color)
endif()

execute_process(
  COMMAND ${command_args}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error_output
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
)

if(NOT result EQUAL 0)
  message(
    FATAL_ERROR
      "dogfood command failed with exit code ${result}\nstdout:\n${output}\nstderr:\n${error_output}"
  )
endif()

if(error_output)
  message(FATAL_ERROR "dogfood command wrote to stderr:\n${error_output}")
endif()

function(check_matches output)
  cmake_parse_arguments(ARG "" "DESCRIPTION;EXPECT_MATCH" "REGEX" ${ARGN})
  foreach(pattern IN LISTS ARG_REGEX)
    if(ARG_EXPECT_MATCH AND NOT "${output}" MATCHES "${pattern}")
      message(FATAL_ERROR "dogfood output is missing ${ARG_DESCRIPTION}'${pattern}':\n${output}")
    elseif(NOT ARG_EXPECT_MATCH AND "${output}" MATCHES "${pattern}")
      message(
        FATAL_ERROR "dogfood output unexpectedly contains ${ARG_DESCRIPTION}'${pattern}':\n${output}"
      )
    endif()
  endforeach()
endfunction()

check_matches("${output}" 
  DESCRIPTION "error output"
  EXPECT_MATCH FALSE 
  REGEX "^error: ")
check_matches("${output}"
  DESCRIPTION "common output"
  EXPECT_MATCH TRUE
  REGEX ${common_regexes})
check_matches("${output}"
  DESCRIPTION "verbose output"
  EXPECT_MATCH ${expect_verbose}
  REGEX ${verbose_regexes})
check_matches("${output}"
  DESCRIPTION "color output"
  EXPECT_MATCH ${LWYI_EXPECT_COLOR}
  REGEX ${color_regexes})
check_matches("${output}"
  DESCRIPTION "debug output"
  EXPECT_MATCH ${expect_debug}
  REGEX ${debug_regexes})
