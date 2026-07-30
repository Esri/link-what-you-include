# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_library(lib_util)
target_sources(lib_util
  PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/util/arg_parser.hpp
    include/util/parallel_transformer.hpp
    include/util/utils.hpp
  PRIVATE
    src/parallel_transformer.cpp
    src/utils.cpp
  )

if(BUILD_TESTING)
  add_executable(lib_util_test)
  target_sources(lib_util_test
    PRIVATE
      test/arg_parser_test.cpp
      test/parallel_transformer_test.cpp
      test/utils_test.cpp
    )
  target_link_libraries(lib_util_test
    PRIVATE
      lib_util
      Catch2::Catch2WithMain
    )
  catch_discover_tests(lib_util_test ADD_TAGS_AS_LABELS)
endif()
