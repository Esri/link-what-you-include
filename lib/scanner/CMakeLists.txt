# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_library(lib_scanner)
target_sources(lib_scanner
  PUBLIC FILE_SET interface_headers TYPE HEADERS BASE_DIRS include FILES
    include/scanner/include.hpp
    include/scanner/scan.hpp
  PRIVATE FILE_SET private_headers TYPE HEADERS FILES
    src/executable_path.hpp
    src/merge_includes.hpp
    src/scan_impl.hpp
  PRIVATE
    src/executable_path.cpp
    src/merge_includes.cpp
    src/scan.cpp
    src/scan_impl.cpp
  )
target_link_libraries(lib_scanner
  PRIVATE
    lib_message
    lib_util
    lib_target_model
    clangDependencyScanning
    clangTooling
  )

# copy/install the clang resource directory into our build/install trees

if(IS_ABSOLUTE ${CMAKE_INSTALL_DATADIR})
  message(FATAL_ERROR "An absolute path for CMAKE_INSTALL_DATADIR is not supported")
endif()

get_target_property(clang_exe clang LOCATION)
message(STATUS "Using clang: ${clang_exe}")

execute_process(
  COMMAND ${clang_exe} -print-resource-dir
  OUTPUT_VARIABLE clang_resource_dir)
string(STRIP ${clang_resource_dir} clang_resource_dir)
cmake_path(SET clang_resource_dir ${clang_resource_dir})
if(NOT EXISTS ${clang_resource_dir}/include)
  message(FATAL_ERROR "Clang resource directory not found")
endif()
message(STATUS "Found clang resource directory: \"${clang_resource_dir}\"")

message(STATUS "Copying clang resource include directory to  \"${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/lwyi\"")
file(COPY ${clang_resource_dir}/include DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/lwyi)
install(DIRECTORY ${clang_resource_dir}/include DESTINATION ${CMAKE_INSTALL_DATADIR}/lwyi)

# Compute the relative path from BINDIR to DATADIR and make it available via a generated header
# file. We use the install paths but this relative path should also work for the build tree because
# of how we laid it out in the root CMakeLists.txt.
set(generated_header ${CMAKE_CURRENT_BINARY_DIR}/relative_resource_dir.hpp)
set(relative_resource_dir ${CMAKE_INSTALL_FULL_DATADIR}/lwyi)
cmake_path(RELATIVE_PATH relative_resource_dir BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR})
message(STATUS "Setting relative path to resource directory to \"${relative_resource_dir}")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/relative_resource_dir.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/relative_resource_dir.hpp
  @ONLY
  )
target_sources(lib_scanner
  PRIVATE FILE_SET private_generated_headers TYPE HEADERS BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} FILES
    ${generated_header}
  )


if(BUILD_TESTING)
  add_executable(lib_scanner_test)
  target_sources(lib_scanner_test
    PRIVATE test/scan_test.cpp
    )
  # allow access to private headers
  target_include_directories(lib_scanner_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
  target_link_libraries(lib_scanner_test
    PRIVATE
      lib_scanner
      lib_target_model
      Catch2::Catch2WithMain
      clangTooling
    )
  catch_discover_tests(lib_scanner_test ADD_TAGS_AS_LABELS)
endif()

if(ENABLE_DOGFOODING)
  link_what_you_include(lib_scanner)
  link_what_you_include(lib_scanner_test)
endif()
