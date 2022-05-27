//
// Created by josh on 5/25/22.
//
#include <SQLiteCpp/SQLiteCpp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <spirv_reflect.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void uploadFile(SQLite::Database& db, const fs::path& path);
static void parseArgs(int argc, char** argv, std::vector<fs::path>& sources, fs::path& out);

int main(int argc, char** argv) {
    fs::path out = "./materials.db";
    std::vector<fs::path> sources;
    parseArgs(argc, argv, sources, out);
    SQLite::Database db(out);
    for (const auto& source : sources) {
        SQLite::Transaction trans(db);
        uploadFile(db, source);
        trans.commit();
    }
}

static std::string getName(const fs::path& path) {
    fs::path name;
    while (name.has_extension())
        name = path.stem();
    return name;
}

static void uploadFile(SQLite::Database& db, const fs::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    stream.close();
    SQLite::Statement stmt(db, "INSERT OR REPLACE INTO shader (name, stage_flag, spv) VALUES (?, ?, ?);");
    auto name = getName(path);
    stmt.bind(1, name);

    spv_reflect::ShaderModule module(data);
    auto stage = module.GetShaderStage();
    stmt.bind(2, stage);

    stmt.bind(3, data.data(), (int)data.size());
    // todo other stuff
    stmt.exec();
}

static void parseArgs(int argc, char** argv, std::vector<fs::path>& sources, fs::path& out) {
    if (argc <= 1) {
        std::cout << "Usage: SpvToDb <FILES...> -o <OUTPUT>\n Files may be .spv files or directories containing .spv "
                  << "files" << std::endl
                  << "The -o option refers to the path to the SQLITE database, if not provided it is assumed "
                  << "to be " << out << std::endl;
        exit(-1);
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i < argc) {
                out = argv[i];
            }
            else {
                std::cerr << "-o option specified without providing a output file" << std::endl;
                exit(-1);
            }
        }
        else {
            fs::path path(argv[i]);
            if (is_directory(path)) {
                for (const auto& entry : fs::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        if (!(entry.path().extension() == ".spv"))
                            std::cerr << "Error: file " << entry.path() << " is not a SPIR-V (.spv) file" << std::endl;
                        else
                            sources.push_back(entry.path());
                    }
                }
            }
            else if (is_regular_file(path)) {
                if (!(path.extension() == ".spv"))
                    std::cerr << "Error: file " << path << " is not a SPIR-V (.spv) file" << std::endl;
                else
                    sources.push_back(path);
            }
        }
    }
}