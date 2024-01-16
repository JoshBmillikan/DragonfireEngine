find_package(fastgltf CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)

set(VULKAN_SRC
        rendering/vulkan/vulkan_renderer.cpp
        rendering/vulkan/vulkan_renderer.h
        rendering/vulkan/context.cpp
        rendering/vulkan/context.h
        rendering/vulkan/vulkan_headers.h)
add_library(dragonfire-client STATIC
        app.cpp
        app.h
        rendering/base_renderer.cpp
        rendering/base_renderer.h
        ${VULKAN_SRC}
)
target_link_libraries(dragonfire-client PUBLIC dragonfire-core fastgltf::fastgltf)
target_link_libraries(dragonfire-client PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator unofficial::spirv-reflect::spirv-reflect)

add_executable(${APP_NAME} main.cpp)
target_link_libraries(${APP_NAME} PRIVATE dragonfire-client SDL2::SDL2main)