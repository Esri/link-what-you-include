# usage: clang_tidy_support()
#
# Configure clang_tidy support.
function(clang_tidy_support)
  if(NOT CMAKE_CXX_CLANG_TIDY)
    find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
    if(CLANG_TIDY_COMMAND)
      message(STATUS "Found clang-tidy: ${CLANG_TIDY_COMMAND}")
    else()
      message(FATAL_ERROR "clang-tidy was not found")
    endif()

    set(options --header-filter=${CMAKE_SOURCE_DIR})
    if(CMAKE_COLOR_DIAGNOSTICS)
      list(APPEND options --use-color)
    endif()
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_COMMAND} ${options} PARENT_SCOPE)
  endif()

  find_program(CLANG_APPLY_REPLACEMENTS_COMMAND NAMES clang-apply-replacements)
  if(CLANG_APPLY_REPLACEMENTS_COMMAND)
    message(STATUS "Found clang-apply-replacements: ${CLANG_APPLY_REPLACEMENTS_COMMAND}")
    set(CMAKE_CXX_CLANG_TIDY_EXPORT_FIXES_DIR "${CMAKE_BINARY_DIR}/clang_tidy_fixes" PARENT_SCOPE)
    add_custom_target(
      apply_replacements
      COMMAND ${CLANG_APPLY_REPLACEMENTS_COMMAND} --style=file --style-config=${CMAKE_SOURCE_DIR}
      "${CMAKE_BINARY_DIR}/clang_tidy_fixes"
      COMMENT "Running clang-apply-replacements"
    )
  else()
    message(STATUS "Did not find clang-apply-replacements. Support for apply_replacements will be disabled.")
  endif()
endfunction()
