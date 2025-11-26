# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

# Usage:
#
#   link_what_you_include(<library_target> [include_prefix...])
#   link_what_you_include(<executable_target>)
#
# This function collects high-level build system information for the given target for use by the
# link-what-you-include tool which can check that a set of libraries and executables have correctly
# specified their dependencies with respect to each other. Call this function for each target that
# you want to be checked.
#
# The link-what-you-include tool scans source files for `#include`'s and maps them to the libraries
# they are associated with. This works best when the dependent target has the INTERFACE_HEADER_SETS
# properties defined but this is not required. To add support for a library that does not have this
# property set, one or more include prefix strings can be provided to disambiguate headers when
# multiple libraries share the same INTERFACE_INCLUDE_DIRECTORIES.
#
function(link_what_you_include target)
  set_property(GLOBAL APPEND PROPERTY LWYI__targets ${target})
  if(ARGN)
    get_target_property(aliased ${target} ALIASED_TARGET)
    if(aliased)
      message(FATAL_ERROR "Setting an include prefix is not currently supported for an alias target")
    endif()
    set_property(TARGET ${target} PROPERTY LWYI__prefixes ${ARGN})
  endif()
endfunction()

define_property(GLOBAL PROPERTY LWYI__targets)
define_property(TARGET PROPERTY LWYI__prefixes)

function(lwyi__list_to_array_element name target list)
  if(${list})
    set(expr "$<TARGET_GENEX_EVAL:${target},${${list}}>")
    set(expr "$<LIST:SORT,${expr}>")
    set(expr "$<LIST:REMOVE_DUPLICATES,${expr}>")
    set(expr "$<LIST:TRANSFORM,${expr},PREPEND,\">")
    set(expr "$<LIST:TRANSFORM,${expr},APPEND,\">")
    set(expr "$<JOIN:${expr},$<COMMA> >")
    set(${list} "\"${name}\": [${expr}]" PARENT_SCOPE)
  else()
    set(${list} PARENT_SCOPE)
  endif()
endfunction()

function(lwyi__write_json)
  # We currently don't support mulit-config generators because the cmake generated
  # compile_commands.json includes the commands for all configurations and there is no way for the
  # link-what-you-include tool to know which commands correspond to a particular configuration.
  get_property(generator_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(generator_is_multi_config)
    message(
      FATAL_ERROR "The cmake support for link-what-you-include currently only supports single-configuration generators"
      )
  endif()

  set(content)
  get_property(targets GLOBAL PROPERTY LWYI__targets)
  foreach(target ${targets})

    # sources
    get_target_property(sources ${target} SOURCES)
    if(sources)
      get_target_property(source_dir ${target} SOURCE_DIR)
      get_target_property(binary_dir ${target} BINARY_DIR)
      set(absolute_sources)
      foreach(source ${sources})
        if((NOT source MATCHES "^ *\\$<[^>]*>") AND (NOT IS_ABSOLUTE "${source}"))
          get_property(
            is_generated
            SOURCE ${source} TARGET_DIRECTORY ${target}
            PROPERTY GENERATED
            )
          if(is_generated)
            cmake_path(APPEND binary_dir "${source}" OUTPUT_VARIABLE source)
          else()
            cmake_path(APPEND source_dir "${source}" OUTPUT_VARIABLE source)
          endif()
        endif()
        list(APPEND absolute_sources ${source})
      endforeach()
      set(sources $<PATH:CMAKE_PATH,NORMALIZE,${absolute_sources}>)
    endif()

    # interface_headers
    get_target_property(interface_header_sets ${target} INTERFACE_HEADER_SETS)
    set(interface_headers)
    if(interface_header_sets)
      foreach(set ${interface_header_sets})
        get_target_property(headers ${target} HEADER_SET_${set})
        if(headers)
          list(APPEND interface_headers $<PATH:CMAKE_PATH,NORMALIZE,${headers}>)
        endif()
      endforeach()
    endif()

    # interface_include_directories
    get_target_property(interface_include_directories ${target} INTERFACE_INCLUDE_DIRECTORIES)
    if(interface_include_directories)
      set(interface_include_directories $<PATH:CMAKE_PATH,NORMALIZE,${interface_include_directories}>)
    endif()

    # interface_dependencies
    get_target_property(interface_dependencies ${target} INTERFACE_LINK_LIBRARIES)
    if(interface_dependencies)
      list(FILTER interface_dependencies EXCLUDE REGEX "\\$<LINK_ONLY:([^>]*)>")
    endif()

    # dependencies
    get_target_property(dependencies ${target} LINK_LIBRARIES)
    if(dependencies)
      list(FILTER dependencies EXCLUDE REGEX "\\$<LINK_ONLY:([^>]*)>")
    endif()

    # interface_include_prefixes
    get_target_property(interface_include_prefixes ${target} LWYI__prefixes)

    # verify_interface_header_sets_sources
    set(verify_interface_header_sets_sources
        "$<$<TARGET_EXISTS:${target}_verify_interface_header_sets>:$<TARGET_PROPERTY:${target}_verify_interface_header_sets,SOURCES>>"
      )

    lwyi__list_to_array_element("interface_headers" ${target} interface_headers)
    lwyi__list_to_array_element("interface_include_directories" ${target} interface_include_directories)
    lwyi__list_to_array_element("interface_include_prefixes" ${target} interface_include_prefixes)
    lwyi__list_to_array_element("interface_dependencies" ${target} interface_dependencies)
    lwyi__list_to_array_element("dependencies" ${target} dependencies)
    lwyi__list_to_array_element("sources" ${target} sources)
    lwyi__list_to_array_element("verify_interface_header_sets_sources" ${target} verify_interface_header_sets_sources)

    string(
      JOIN
      ",\n"
      object_content
      ${interface_headers}
      ${interface_include_directories}
      ${interface_include_prefixes}
      ${interface_dependencies}
      ${dependencies}
      ${sources}
      ${verify_interface_header_sets_sources}
      )

    list(APPEND content "\"${target}\": {\n${object_content}\n}")
  endforeach()

  string(JOIN ",\n" content ${content})
  string(PREPEND content "{\n")
  string(APPEND content "\n}\n")

  message(STATUS "Writing link_what_you_include_info.json file")
  file(
    GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/link_what_you_include_info.json"
    CONTENT "${content}"
    )
endfunction()

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL lwyi__write_json)
