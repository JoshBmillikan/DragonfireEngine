//
// Created by josh on 5/21/22.
//

#pragma once
#include "Engine.h"
#include <filesystem>

namespace dragonfire {
    /// \brief Reads a binary file into a vector of bytes
    /// \param path the path to the file to read
    /// \return vector of unsigned chars containing the file's data
    [[nodiscard]] EXPORTED std::vector<unsigned char> readFileToBytes(const std::filesystem::path& path);
    [[nodiscard]] EXPORTED std::vector<unsigned char> readLZ4FileToBytes(const std::filesystem::path& path);
}   // namespace dragonfire
