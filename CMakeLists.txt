# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ryan Crepa

cmake_minimum_required(VERSION 3.16)

project(sprdwatchrooter-next LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sprdwatchrooter-next
    main.cpp
)

target_include_directories(sprdwatchrooter-next PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_sources(sprdwatchrooter-next PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/file_io.cpp
)

add_library(file_io-lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/file_io.cpp
)

target_link_libraries(sprdwatchrooter-next PRIVATE file_io-lib)

if(MSVC OR WIN32)
    target_compile_options(sprdwatchrooter-next PRIVATE /utf-8)
endif()

