//
// Created by josh on 5/19/22.
//

#include "Material.h"
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <shaderc/shaderc.hpp>
#include <nlohmann/json.hpp>

inline void check(auto& result) {
    if (result.GetNumErrors() != 0) {
        std::cerr << "Error compiling shader: " << result.GetErrorMessage() << std::endl;
        exit(-1);
    }
}

void dragonfire::compileMaterial(
        const std::filesystem::path& outFile,
        const std::string& vertShader,
        const std::string& fragShader,
        const std::string& geometryShader
) {
    static shaderc::Compiler compiler;
    if (vertShader.empty()) {
        std::cerr << "Empty vertex shader source" << std::endl;
        exit(-1);
    }
    if (fragShader.empty()) {
        std::cerr << "Empty fragment shader source" << std::endl;
        exit(-1);
    }

    std::ofstream out(outFile, std::ios::out | std::ios::binary);
    nlohmann::json header;
    header["littleEndian"] = std::endian::native == std::endian::little;

    auto result = compiler.CompileGlslToSpv(vertShader, shaderc_vertex_shader, "vertex shader");
    check(result);

    out << static_cast<uint64_t>(std::distance(result.begin(), result.end()));
    for (const uint32_t data : result)
        out << data;

    result = compiler.CompileGlslToSpv(fragShader, shaderc_fragment_shader, "fragment shader");
    check(result);

    out << static_cast<uint64_t>(std::distance(result.begin(), result.end()));
    for(const uint32_t data : result)
        out << data;

    if(geometryShader.empty())
        out << static_cast<uint64_t>(0u);
    else {
        result = compiler.CompileGlslToSpv(geometryShader,shaderc_geometry_shader, "geometry shader");
        check(result);

        out << static_cast<uint64_t>(std::distance(result.begin(),result.end()));
        for(const uint32_t data : result)
            out << data;
    }
    out.flush();
}
