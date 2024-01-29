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
        rendering/vulkan/vulkan_headers.h
        rendering/vulkan/allocation.cpp
        rendering/vulkan/allocation.h
        rendering/vulkan/swapchain.cpp
        rendering/vulkan/swapchain.h
        rendering/vulkan/pipeline.cpp
        rendering/vulkan/pipeline.h
        rendering/vulkan/mesh.cpp
        rendering/vulkan/mesh.h
        rendering/vulkan/descriptor_set.cpp
        rendering/vulkan/descriptor_set.h
        rendering/vulkan/texture.cpp
        rendering/vulkan/texture.h
        rendering/vulkan/gltf_loader.cpp
        rendering/vulkan/gltf_loader.h)
add_library(dragonfire-client STATIC
        app.cpp
        app.h
        rendering/base_renderer.cpp
        rendering/base_renderer.h
        ${VULKAN_SRC}
        rendering/vertex.h
        rendering/camera.cpp
        rendering/camera.h
        rendering/material.cpp
        rendering/material.h
        rendering/base_gltf_loader.h
)
target_link_libraries(dragonfire-client PUBLIC dragonfire-core fastgltf::fastgltf)
target_link_libraries(dragonfire-client PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator unofficial::spirv-reflect::spirv-reflect)
add_dependencies(dragonfire-client shader-target)
target_compile_definitions(dragonfire-client PRIVATE "SHADER_OUTPUT_PATH=\"${SHADERS_OUT}\"")

add_executable(${APP_NAME} main.cpp)
target_link_libraries(${APP_NAME} PRIVATE dragonfire-client SDL2::SDL2main)