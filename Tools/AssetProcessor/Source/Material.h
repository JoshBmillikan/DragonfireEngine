//
// Created by josh on 5/19/22.
//

#pragma once
#include <string>
#include <filesystem>

namespace dragonfire {

void compileMaterial(
        const std::filesystem::path& outFile,
        const std::string& vertShader,
        const std::string& fragShader,
        const std::string& geometryShader = ""
);
}