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
        const std::string& definition,
        const std::string& vertShader,
        const std::string& fragShader,
        const std::string& geometryShader
) {
    static shaderc::Compiler compiler;

    std::ofstream out(outFile, std::ios::out | std::ios::binary);
    nlohmann::json header = definition;
    header["littleEndian"] = std::endian::native == std::endian::little;

    out << header;
    out << '\0';
    //todo
}
