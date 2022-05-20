cmake_minimum_required(VERSION 3.22)
project(DragonfireEngine)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        add_compile_options(/EHsc)
    endif()
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
add_subdirectory(Tools)
add_subdirectory(ThirdParty)
add_subdirectory(Engine)
add_subdirectory(Client)
add_subdirectory(Server)
file(COPY Config DESTINATION ${CMAKE_BINARY_DIR})
file(COPY Assets DESTINATION ${CMAKE_BINARY_DIR})