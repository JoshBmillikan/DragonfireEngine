
add_library(Client STATIC Source/GameClient.cpp Source/GameClient.h)
add_executable(Dragonfire Main.cpp)
target_include_directories(Client PUBLIC Source)
target_link_libraries(Client PUBLIC Engine SDL2::SDL2 Rendering)
target_link_libraries(Dragonfire PUBLIC Client SDL2::SDL2main)
target_link_libraries(Client PRIVATE VulkanRendering)
target_precompile_headers(Client REUSE_FROM Engine)
add_subdirectory(Rendering)