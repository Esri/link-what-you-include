# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.29.2)

project(
  lwyi
  DESCRIPTION "Link what you include"
  LANGUAGES CXX 
  )

################################################################################
## settings

include(CTest)
include(CMakeDependentOption)
include(GNUInstallDirs)

cmake_dependent_option(ENABLE_DOGFOODING "Enable support for running link-what-you-include on itself" ON BUILD_TESTING OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wimplicit-fallthrough
    )
endif()

option(ENABLE_CLANG_TIDY "Run clang-tidy while building" FALSE)
if(ENABLE_CLANG_TIDY)
  include(cmake/clang_tidy_support.cmake)
  clang_tidy_support()
endif()

################################################################################
## configure build tree to mirror install tree

if(IS_ABSOLUTE ${CMAKE_INSTALL_BINDIR})
  message(FATAL_ERROR "An absolute path for CMAKE_INSTALL_BINDIR is not supported")
endif()

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(is_multi_config)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}_$<LOWER_CASE:$<CONFIG>>)
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
endif()

################################################################################
## dependencies

enable_language(C) # needed for clang
find_package(Clang REQUIRED)
target_include_directories(clangTooling SYSTEM INTERFACE ${CLANG_INCLUDE_DIRS})
if (MSVC)
  target_compile_options(clangTooling INTERFACE /GR-)
else()
  target_compile_options(clangTooling INTERFACE -fno-rtti)
endif()

find_package(fmt REQUIRED)
find_package(tl-expected REQUIRED)
find_package(simdjson REQUIRED)

if(BUILD_TESTING)
  find_package(Catch2 REQUIRED)
  include(Catch)
endif()

if(ENABLE_DOGFOODING)
  include(cmake/link_what_you_include.cmake)

  link_what_you_include(clangTooling clang)
  link_what_you_include(fmt::fmt fmt)
  link_what_you_include(tl::expected tl)
endif()

################################################################################
## subdirectories

add_subdirectory(lib/lwyi)
add_subdirectory(lib/scanner)
add_subdirectory(lib/target_model)
add_subdirectory(lib/tidy)
add_subdirectory(lib/util)
add_subdirectory(app)

