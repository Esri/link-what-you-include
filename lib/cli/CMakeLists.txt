# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_library(lib_cli)
target_sources(lib_cli
  PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/cli/command_options.hpp
    include/cli/parse_arguments.hpp
  PRIVATE
    src/parse_arguments.cpp
  )
target_link_libraries(lib_cli
  PUBLIC
    lib_message
  PRIVATE
    lib_util
  )

if(BUILD_TESTING)
  add_executable(lib_cli_test)
  target_sources(lib_cli_test
    PRIVATE
      test/parse_arguments_test.cpp
    )
  target_link_libraries(lib_cli_test
    PRIVATE
      lib_cli
      lib_message
      Catch2::Catch2WithMain
    )
  catch_discover_tests(lib_cli_test ADD_TAGS_AS_LABELS)
endif()

if(ENABLE_DOGFOODING)
  link_what_you_include(lib_cli)
  link_what_you_include(lib_cli_test)
endif()
