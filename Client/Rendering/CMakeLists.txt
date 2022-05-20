
add_library(Rendering INTERFACE Interface/Interface.h Interface/IMaterial.h Interface/BaseMesh.h)
target_include_directories(Rendering INTERFACE Interface)
target_link_libraries(Rendering INTERFACE SDL2::SDL2 glm::glm)
add_subdirectory(Vulkan)