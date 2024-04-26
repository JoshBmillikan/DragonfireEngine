//
// Created by josh on 1/13/24.
//
#include <magic_enum.hpp>
#include <magic_enum_iostream.hpp>

using magic_enum::iostream_operators::operator<<;
using magic_enum::iostream_operators::operator>>;

#include "crash.h"
#include "engine.h"
#include "file.h"
#include "utility/frame_allocator.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <iostream>
#include <physfs.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace dragonfire {

Engine* Engine::INSTANCE = nullptr;

static void initLogging(spdlog::level::level_enum level)
{
    using namespace spdlog;
    if (const char* str = std::getenv("LOG_LEVEL"))
        level = level::from_str(str);

    const char* writeDir = PHYSFS_getWriteDir();
    assert("Please initialize PhysFs write path before logging" && writeDir);
    std::string logPath = writeDir;
    logPath.append("log.txt");
    init_thread_pool(8192, 1);

    auto stdoutSink = std::make_shared<sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<sinks::basic_file_sink_mt>(logPath);
    const auto logger = std::make_shared<async_logger>(
        "global",
        sinks_init_list{stdoutSink, fileSink},
        thread_pool(),
        async_overflow_policy::block
    );
    register_logger(logger);
    set_default_logger(logger);
    set_level(level);
    set_pattern("[%T] [%^%=8!l%$] [%=12!n] %v %@");
    info("Logging started to {}", logPath);
}

static void mountDir(const std::string& str)
{
    const auto delim = str.find_first_of('=');
    if (delim == std::string::npos) {
        std::cerr << "Invalid mount directory " << str
                  << ", arguments must be given in the form of [dir]=[mount point]" << std::endl;
        return;
    }
    const auto first = str.substr(0, delim);
    const auto last = str.substr(delim + 1);
    if (PHYSFS_mount(first.c_str(), last.c_str(), 1) == 0)
        std::cerr << "Failed to mount directory \"" << first << '"'
                  << ", error: " << PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()) << std::endl;
    else
        std::cout << "mounted dir" << first << " to " << last << std::endl;
}

Engine::Engine(
    const bool remote,
    const int argc,
    char** argv
)
    : remote(remote), argc(argc), argv(argv)
{
    INSTANCE = this;
}

void Engine::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_HAPTIC | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
        crash("SDL init failed: {}", SDL_GetError());
    crashOnException([this] { File::init(argc, argv); });
    parseCommandLine();
    if (cli["mount"].count() > 0) {
        for (auto& opt : cli["mount"].as<std::vector<std::string>>())
            mountDir(opt);
    }
    try {
        const auto path = std::filesystem::current_path().append("assets");
        File::mount(path.c_str(), nullptr);
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to mount asset dir: {}", e.what());
    }
    initLogging(cli["log"].as<spdlog::level::level_enum>());
    lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::string, sol::lib::math);
    spdlog::info("lua interpreter version: {}", lua.get_or<std::string>("_VERSION", "Unknown"));
}

Engine::~Engine()
{
    spdlog::info("Goodbye!");
    SDL_Quit();
}

void Engine::run()
{
    crashOnException([this] {
        Uint64 time = SDL_GetTicks64();
        running = true;
        while (running) {
            frameAllocator::nextFrame();
            const Uint64 now = SDL_GetTicks64();
            const double deltaTime = static_cast<double>(now - time) / 1000.0;
            time = now;
            mainLoop(deltaTime);
        }
    });
}

void Engine::parseCommandLine()
{
    cxxopts::Options options(APP_NAME, "A voxel game engine");
    auto adder = options.add_options();
    getExtraCliOptions(std::move(adder))("h,help", "Print usage")(
        "l,log",
        "Log level [trace, debug, info, warn, err, critical, off]",
        cxxopts::value<spdlog::level::level_enum>()->default_value("info")
    )("m,mount", "Mount a directory to a virtual mount point, specify args in the form of [dir]=[mount point]",
        cxxopts::value<std::vector<std::string>>());

    cli = options.parse(argc, argv);

    if (cli.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }
}
}// namespace dragonfire