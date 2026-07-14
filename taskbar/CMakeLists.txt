cmake_minimum_required(VERSION 3.20)

project(taskbar_controller LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "This project uses Win32 APIs and must be configured on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(taskbar
    main.cpp
    taskbar_controller.cpp
)

target_include_directories(taskbar PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(taskbar PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if (WIN32)
    target_link_libraries(taskbar PRIVATE user32)
endif()

if (MINGW)
    target_link_options(taskbar PRIVATE "-municode")
endif()
