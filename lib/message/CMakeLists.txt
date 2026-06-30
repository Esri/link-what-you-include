# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

add_library(lib_message)
target_sources(lib_message
  PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/message/message.hpp
  PRIVATE
    src/message.cpp
  )

if(ENABLE_DOGFOODING)
  link_what_you_include(lib_message)
endif()
