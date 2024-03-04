cmake_minimum_required(VERSION 3.27)
project(DragonfireEngine)

set(CMAKE_CXX_STANDARD 20)

set(APP_NAME "dragonfire_engine" CACHE STRING "Name of the game executable")
set(APP_DISPLAY_NAME "Dragonfire Engine" CACHE STRING "Display name of the game(window title)")

add_subdirectory(external)

add_compile_definitions("APP_NAME=\"${APP_NAME}\"" "APP_DISPLAY_NAME=\"${APP_DISPLAY_NAME}\"")
add_compile_definitions("$<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>")

option(SANITIZE "Enable address sanitizer" OFF)

if (SANITIZE)
    message("Address sanitizer enabled")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"))
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address -fno-omit-frame-pointer)
    else ()
        message("Address sanitizer is only supported on gcc and clang")
    endif ()
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
add_subdirectory(src)