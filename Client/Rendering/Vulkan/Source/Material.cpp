//
// Created by josh on 5/19/22.
//

#include "Material.h"
#include <nlohmann/json.hpp>
using namespace dragonfire::rendering;
IMaterial* loadMaterial(std::ifstream& stream) { return new Material(stream); }

void read(std::vector<uint32_t>& vec, std::ifstream& stream) {
    uint64_t len;
    stream >> len;
    if (len > 0) {
        vec.reserve(len);
        for (auto i = 0; i < len; i++) {
            uint32_t word;
            stream >> word;
            vec.push_back(word);
        }
    }
}

nlohmann::json readJson(std::ifstream& stream) {
    std::string header;
    std::getline(stream, header, '\0');
    return header;
}

Material::Material(std::ifstream& stream) {
    std::vector<uint32_t> vertex, fragment, geometry;
    auto header = readJson(stream);

    bool isLittleEndian = header["littleEndian"];
    if ((isLittleEndian && std::endian::native == std::endian::little)
        || (isLittleEndian == 0 && std::endian::native == std::endian::big)) {
        // todo handle endian miss-match
    }
    else {
        read(vertex, stream);
        read(fragment, stream);
        read(geometry, stream);
    }
}
