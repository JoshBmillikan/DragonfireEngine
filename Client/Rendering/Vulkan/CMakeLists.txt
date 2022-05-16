
add_library(VulkanRendering STATIC Source/RenderingEngine.cpp Source/RenderingEngine.h Source/Interface.cpp Source/Initialization.cpp Source/Macros.h)

find_package(Vulkan REQUIRED)
target_link_libraries(VulkanRendering PRIVATE Rendering spdlog::spdlog Vulkan::Headers VulkanMemoryAllocator Engine)
target_precompile_headers(VulkanRendering PRIVATE "Source/Macros.h" <vector> <algorithm> <memory> <tuple> <spdlog/spdlog.h> <glm/glm.hpp> <cstdint> <string> <vulkan/vulkan.hpp>)