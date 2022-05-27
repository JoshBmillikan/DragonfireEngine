find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

file(GLOB SHADER_SRC Source/*)

foreach(src IN LISTS ${SHADER_SRC})
    cmake_path(GET ${src} FILENAME fname)
    set(OUTFILE ${CMAKE_BINARY_DIR}/Shaders/${fname}.spv)
    add_custom_command(
            OUTPUT ${OUTFILE}
            DEPENDS ${src}
            DEPFILE ${src}.d
            COMMAND ${glslc_executable}
            ${src}
            -o ${OUTFILE}
            COMMENT "Compiling ${fname}"
    )
    list(APPEND SPV_SHADERS ${OUTFILE})
    message("Configured to build shader ${src}")
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_SHADERS})