set(SDL_TEST_ENABLED_BY_DEFAULT OFF)
add_subdirectory(SDL EXCLUDE_FROM_ALL)
set(TARGET_HELLO_WORLD OFF)
set(TARGET_UNIT_TESTS OFF)
set(TARGET_PERFORMANCE_TEST OFF)
set(TARGET_VIEWER OFF)
set(TARGET_SAMPLES OFF)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY=${USE_STATIC_CRT})
set($<$<CONFIG:Debug>:INTERPROCEDURAL_OPTIMIZATION OFF)
set(DOUBLE_PRECISION ON)
add_subdirectory(Jolt/Build)

add_library(imgui STATIC imgui/imgui.cpp
        imgui/imgui.h
        imgui/imconfig.h
        imgui/imgui_draw.cpp
        imgui/imgui_internal.h
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/imstb_rectpack.h
        imgui/imstb_textedit.h
        imgui/imstb_truetype.h)

target_include_directories(imgui PUBLIC imgui imgui/backends)
add_library(imgui_SDL2 STATIC imgui/backends/imgui_impl_sdl2.h imgui/backends/imgui_impl_sdl2.cpp)
add_library(imgui_vulkan STATIC imgui/backends/imgui_impl_vulkan.h imgui/backends/imgui_impl_vulkan.cpp)
target_link_libraries(imgui_SDL2 PUBLIC imgui SDL2::SDL2)
find_package(Vulkan REQUIRED)
target_link_libraries(imgui_vulkan PUBLIC imgui Vulkan::Headers)
target_compile_definitions(imgui_vulkan PRIVATE VK_NO_PROTOTYPES IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "Build Noise Tool" FORCE)
add_subdirectory(FastNoise2)