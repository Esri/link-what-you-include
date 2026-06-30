# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_library(lib_target_model)
target_sources(lib_target_model
  PUBLIC FILE_SET interface_headers TYPE HEADERS BASE_DIRS include FILES
    include/target_model/target.hpp
    include/target_model/target_data.hpp
    include/target_model/target_model.hpp
    include/target_model/target_model_loader.hpp
  PRIVATE FILE_SET private_headers TYPE HEADERS FILES
    src/file_loader.hpp
    src/real_file_loader.hpp
    src/target_model_loader_impl.hpp
  PRIVATE
    src/real_file_loader.cpp
    src/target_data.cpp
    src/target_model.cpp
    src/target_model_loader_impl.cpp
  )
target_link_libraries(lib_target_model
  PRIVATE
    lib_util
    simdjson::simdjson
  )

if(BUILD_TESTING)
  add_executable(lib_target_model_test)
  target_sources(lib_target_model_test
    PRIVATE
      test/target_model_loader_impl_test.cpp
      test/target_data_test.cpp
    )
  # allow access to private headers
  target_include_directories(lib_target_model_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
  target_link_libraries(lib_target_model_test
    PRIVATE
      lib_target_model
      Catch2::Catch2WithMain
      simdjson::simdjson
    )
  catch_discover_tests(lib_target_model_test ADD_TAGS_AS_LABELS)
endif()

if(ENABLE_DOGFOODING)
  link_what_you_include(lib_target_model)
  link_what_you_include(lib_target_model_test)
endif()
