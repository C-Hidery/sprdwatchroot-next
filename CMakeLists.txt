# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ryan Crepa

cmake_minimum_required(VERSION 3.16)

project(sprdwatchrooter-next LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出文件名
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(Start
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/file_io.cpp
)

target_include_directories(Start PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

if(MSVC OR WIN32)
    target_compile_options(Start PRIVATE /utf-8)
    # 确保输出文件名为 Start.exe
    set_target_properties(Start PROPERTIES
        OUTPUT_NAME "Start"
        SUFFIX ".exe"
    )
else()
    # 非Windows平台也设置输出名称
    set_target_properties(Start PROPERTIES
        OUTPUT_NAME "Start"
    )
endif()

# 可选：设置版本信息
set_target_properties(Start PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)