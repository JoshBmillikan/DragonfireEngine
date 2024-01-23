include_directories(.)

find_package(Catch2 CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

add_subdirectory(core)
add_subdirectory(shaders)
add_subdirectory(client)
add_subdirectory(server)