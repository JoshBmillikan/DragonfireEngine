set(SHADERS glsl/default.frag glsl/default.vert glsl/cull.comp)

find_package(Vulkan COMPONENTS glslc REQUIRED)
set(SHADERS_OUT ${CMAKE_BINARY_DIR}/shaders CACHE INTERNAL "")

file(MAKE_DIRECTORY ${SHADERS_OUT})
foreach (SHADER IN LISTS SHADERS)
    get_filename_component(FILENAME ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} NAME)
    add_custom_command(OUTPUT ${SHADERS_OUT}/${FILENAME}.spv
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} -o ${SHADERS_OUT}/${FILENAME}.spv
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            COMMENT "Compiling ${FILENAME}")
    list(APPEND SPV_SHADERS ${SHADERS_OUT}/${FILENAME}.spv)
endForeach ()

add_custom_target(shader-target ALL DEPENDS ${SPV_SHADERS})
