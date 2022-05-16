//
// Created by Josh on 5/14/2022.
//

#include "Game.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include <Service.h>
#include <SDL.h>
#include "FileLocator.h"
#include "Settings.h"
#include "re2/re2.h"

using namespace dragonfire;
using namespace std::chrono;
EXPORTED void Game::run() {
    auto time = steady_clock::now();
    while (running) {
        double delta = duration_cast<duration<double>>(steady_clock::now() - time).count();
        mainLoop(delta);
        time = steady_clock::now();
    }
}

static void initLogging(spdlog::level::level_enum level) {
    using namespace spdlog;
    auto stdoutSink = std::make_shared<sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<sinks::basic_file_sink_mt>(
            std::filesystem::temp_directory_path().append("Dragonfire/EngineLog.txt").string());
    std::vector<sink_ptr> sinks{stdoutSink, fileSink};
    auto logger = std::make_shared<async_logger>(
            "DragonfireEngineLog",
            sinks.begin(),
            sinks.end(),
            thread_pool(),
            async_overflow_policy::block);
    register_logger(logger);
    set_level(level);
    info("Logging started");
    info("Platform: {}", SDL_GetPlatform());
    SDL_version version;
    SDL_GetVersion(&version);
    info("SDL Version: {}.{}.{}", version.major, version.minor, version.patch);
}

static void parseCLI(int argc, char** argv) {
    // matches CLI args, eg --foo or --foo=bar
    RE2 regex(R"(^-?-([A-Za-z_0-9\.]+) *= *([-_A-Za-z0-9\.]+) *$|--([A-Za-z_0-9\.]+) *$)");
    auto& settings = Service::get<Settings>();
    for(int i=0;i<argc;i++) {
        re2::StringPiece name, value;
        std::string arg(argv[i]);
        if (RE2::FullMatch(arg,regex,&name,&value)) {

        } else if (RE2::FullMatch(arg,regex,&name))
            settings.insert(name.as_string(),true);
    }
}

EXPORTED Game::Game(int argc, char** argv, spdlog::level::level_enum level) {
    initLogging(level);
    Service::init<FileLocator>();
    auto& fs = Service::get<FileLocator>();
    Service::init<Settings>();
    parseCLI(argc,argv);
}

EXPORTED Game::~Game() noexcept {
    Service::destroyServices();
    spdlog::info("Game shutdown");
    spdlog::shutdown();
}
