//
// Created by josh on 5/21/22.
//

#include "Util.h"
#include <fstream>
#include <lz4frame.h>

namespace dragonfire {
std::vector<unsigned char> readFileToBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    return {(std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>()};
}

std::vector<unsigned char> readLZ4FileToBytes(const std::filesystem::path& path) {
    auto raw = readFileToBytes(path);
    LZ4F_decompressionContext_t context;
    auto result = LZ4F_createDecompressionContext(&context, LZ4F_VERSION);
    if (LZ4F_isError(result))
        throw std::runtime_error("Failed to create lz4 decompression context");
    std::vector<unsigned char> data;
    unsigned char dest[256];
    size_t destSize = 256, srcSize = raw.size();
    // LZ4F_decompress(context,dest,&destSize,raw.data(),&srcSize,); todo
    return data;
}
}   // namespace dragonfire