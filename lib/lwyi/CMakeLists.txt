# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_library(lib_lwyi)
target_sources(lib_lwyi
  PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/lwyi/check_target.hpp
    include/lwyi/dependency_visibility.hpp
    include/lwyi/strongly_connected_dependencies.hpp
  PRIVATE
    src/check_target.cpp
    src/dependency_visibility.cpp
    src/strongly_connected_dependencies.cpp
  )
target_link_libraries(lib_lwyi
  PUBLIC
    lib_scanner
    lib_target_model
  )

if(BUILD_TESTING)
  add_executable(lib_lwyi_test)
  target_sources(lib_lwyi_test
    PRIVATE
      test/check_target_test.cpp
      test/dependency_visibility_test.cpp
      test/strongly_connected_dependencies_test.cpp
    )
  target_link_libraries(lib_lwyi_test
    PRIVATE
      lib_lwyi
      lib_scanner
      lib_target_model
      Catch2::Catch2WithMain
    )
  catch_discover_tests(lib_lwyi_test ADD_TAGS_AS_LABELS)
endif()

if(ENABLE_DOGFOODING)
  link_what_you_include(lib_lwyi)
  link_what_you_include(lib_lwyi_test)
endif()
