//
// Created by josh on 5/24/22.
//

#include <SQLiteCpp/SQLiteCpp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <shaderc/shaderc.hpp>
#include <spirv_reflect.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

void parseArgs(fs::path& out, std::vector<fs::path>& paths, int argc, char** argv);
std::vector<uint32_t> compile(const fs::path& path);
void uploadToDb(SQLite::Database& db, const std::vector<uint32_t>& spv, const std::string& name);

int main(int argc, char** argv) {
    if (argc <= 1) {
        std::cerr << "No glsl sources provided" << std::endl;
        return -1;
    }
    fs::path out("./materials.db");
    std::vector<fs::path> paths;
    parseArgs(out, paths, argc, argv);
    SQLite::Database db(out);

    SQLite::Transaction trans(db);
    for (const auto& source : paths) {
        auto spv = compile(source);
        uploadToDb(db, spv, source.filename());
    }
    trans.commit();

    return 0;
}

void uploadToDb(SQLite::Database& db, const std::vector<uint32_t>& spv, const std::string& name) {
    SQLite::Statement stmt(db, "INSERT OR REPLACE INTO shader (name, type, spv) VALUES(?, ?, ?);");
    stmt.bind(1, name);
    spv_reflect::ShaderModule module(spv);
    uint32_t varCount;
    module.EnumerateInputVariables(&varCount, nullptr);
    auto vars = std::make_unique<SpvReflectInterfaceVariable*[]>(varCount);
    module.EnumerateInputVariables(&varCount, vars.get());
    //todo
}

std::vector<uint32_t> compile(const fs::path& path) {
    static shaderc::Compiler compiler;
    auto ext = path.extension();
    auto name = path.filename();
    std::ifstream stream(path);
    std::stringstream buf;
    buf << stream.rdbuf();

    shaderc_shader_kind kind = shaderc_glsl_infer_from_source;
    if (ext == ".vert")
        kind = shaderc_vertex_shader;
    else if (ext == ".frag")
        kind = shaderc_fragment_shader;
    else if (ext == ".tesc")
        kind = shaderc_tess_control_shader;
    else if (ext == ".tese")
        kind = shaderc_tess_evaluation_shader;
    else if (ext == ".geom")
        kind = shaderc_geometry_shader;
    else if (ext == ".comp")
        kind = shaderc_compute_shader;

    auto result = compiler.CompileGlslToSpv(buf.str(), kind, name.c_str());
    if (result.GetNumErrors() != 0) {
        std::cerr << "Failed to compile shader" << result.GetErrorMessage() << std::endl;
        exit(-1);
    }

    return {result.begin(), result.end()};
}

void parseArgs(fs::path& out, std::vector<fs::path>& paths, int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            i++;
            if (i < argc)
                out = fs::path(argv[i]);
            else {
                std::cerr << "No output file specified for -o option" << std::endl;
                exit(-1);
            }
        }
        else
            paths.emplace_back(argv[i]);
    }
}