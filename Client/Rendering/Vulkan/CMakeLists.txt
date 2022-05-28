
add_library(VulkanRendering STATIC Source/RenderingEngine.cpp Source/RenderingEngine.h Source/Interface.cpp Source/Initialization.cpp Source/Macros.h Source/Material.cpp Source/Material.h Source/Mesh.cpp Source/Mesh.h Source/Allocation.cpp Source/Allocation.h Source/Shader.cpp Source/Shader.h)

find_package(Vulkan REQUIRED)
target_link_libraries(VulkanRendering PRIVATE Rendering spdlog::spdlog Vulkan::Headers VulkanMemoryAllocator Engine readerwriterqueue)
add_dependencies(VulkanRendering Shaders)
target_precompile_headers(VulkanRendering PRIVATE "Source/Macros.h" <vector> <algorithm> <memory> <tuple> <spdlog/spdlog.h> <glm/glm.hpp> <cstdint> <string> <vulkan/vulkan.hpp>)