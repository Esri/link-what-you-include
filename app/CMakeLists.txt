# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_executable(lwyi)
target_sources(lwyi
  PRIVATE FILE_SET private_headers TYPE HEADERS FILES
    src/tidy_tool.hpp
    src/graph_tool.hpp
    src/run_lwyi.hpp
    src/run_lwyi_on_target.hpp
    src/run_tool.hpp
  PRIVATE
    src/tidy_tool.cpp
    src/graph_tool.cpp
    src/main.cpp
    src/run_lwyi.cpp
    src/run_lwyi_on_target.cpp
    src/run_tool.cpp
  )
target_link_libraries(
  lwyi
  PRIVATE 
    lib_lwyi
    lib_cli
    lib_message
    lib_tidy
    lib_scanner
    lib_target_model
    lib_util
    simdjson::simdjson
  )

install(TARGETS lwyi)

if(ENABLE_DOGFOODING)
  link_what_you_include(lwyi)

  function(add_dogfood_test name)
    cmake_parse_arguments(DOGFOOD "COLOR" "MESSAGE_LEVEL" "" ${ARGN})

    add_test(
      NAME ${name}
      COMMAND ${CMAKE_COMMAND}
        -DLWYI_EXECUTABLE=$<TARGET_FILE:lwyi>
        -DLWYI_BINARY_DIR=${CMAKE_BINARY_DIR}
        -DLWYI_EXPECT_COLOR=${DOGFOOD_COLOR}
        -DLWYI_EXPECT_MESSAGE_LEVEL=${DOGFOOD_MESSAGE_LEVEL}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test/dogfood_test.cmake
    )
    set_tests_properties(${name} PROPERTIES LABELS dogfood)
  endfunction()

  add_dogfood_test(dogfood_test MESSAGE_LEVEL normal)
  add_dogfood_test(dogfood_verbose_test MESSAGE_LEVEL verbose)
  add_dogfood_test(dogfood_color_test COLOR MESSAGE_LEVEL normal)
  add_dogfood_test(dogfood_debug_test MESSAGE_LEVEL debug)
endif()
