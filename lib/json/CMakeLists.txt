# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_library(lib_json INTERFACE)
target_sources(lib_json
  INTERFACE FILE_SET HEADERS BASE_DIRS include FILES
    include/json/json_utils.hpp
  )

target_link_libraries(lib_json
  INTERFACE
    simdjson::simdjson
  )

if(BUILD_TESTING)
  add_executable(lib_json_test test/json_test.cpp)
  target_link_libraries(lib_json_test PRIVATE lib_json Catch2::Catch2WithMain)
  catch_discover_tests(lib_json_test ADD_TAGS_AS_LABELS)
endif()
