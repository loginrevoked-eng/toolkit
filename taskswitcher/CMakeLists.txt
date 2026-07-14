cmake_minimum_required(VERSION 3.16)
project(TaskSwitcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set SDL3 path
set(SDL3_DIR "C:/SDL3-3.4.12")
set(SDL3_ttf_DIR "C:/SDL3_ttf-3.2.2")

# Include SDL3 headers
include_directories("${SDL3_DIR}/include")
include_directories("${SDL3_ttf_DIR}/include")

# Link directories for SDL3 libraries (x64 for 64-bit builds)
link_directories("${SDL3_DIR}/lib/x64" "${SDL3_ttf_DIR}/lib/x64")

# Executable for tasswi.cpp
add_executable(TaskSwitcher tasswi.cpp)
target_link_libraries(TaskSwitcher PRIVATE SDL3 shlwapi)

# Executable for switcher_ui.cpp
add_executable(SwitcherUI switcher_ui.cpp)
target_link_libraries(SwitcherUI PRIVATE SDL3 SDL3_ttf)

# Executable for main.cpp (combines both modules via includes)
add_executable(MainApp main.cpp)
target_link_libraries(MainApp PRIVATE SDL3 SDL3_ttf shlwapi)

# Copy SDL3.dll and SDL3_ttf.dll to build directory after building
add_custom_command(TARGET SwitcherUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_DIR}/lib/x64/SDL3.dll"
    $<TARGET_FILE_DIR:SwitcherUI>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_ttf_DIR}/lib/x64/SDL3_ttf.dll"
    $<TARGET_FILE_DIR:SwitcherUI>
)

add_custom_command(TARGET MainApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_DIR}/lib/x64/SDL3.dll"
    $<TARGET_FILE_DIR:MainApp>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_ttf_DIR}/lib/x64/SDL3_ttf.dll"
    $<TARGET_FILE_DIR:MainApp>
)