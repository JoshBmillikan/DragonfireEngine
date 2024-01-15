cmake_minimum_required(VERSION 3.27)
project(DragonfireEngine)

set(CMAKE_CXX_STANDARD 20)

set(APP_NAME "raven" CACHE STRING "Name of the game executable")
set(APP_DISPLAY_NAME "Raven Engine" CACHE STRING "Display name of the game(window title)")

add_subdirectory(external)
add_compile_definitions("APP_NAME=\"${APP_NAME}\"" "APP_DISPLAY_NAME=\"${APP_DISPLAY_NAME}\"")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
add_subdirectory(src)