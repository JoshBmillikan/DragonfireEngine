//
// Created by Josh on 5/14/2022.
//
#include <GameClient.h>
#include <SDL_main.h>

using namespace dragonfire;

int main(int argc, char** argv) {
    try {
        GameClient game(argc, argv);
        game.run();
    } catch (const std::exception& e) {
        spdlog::error("Uncaught exception: {}", e.what());
        return -1;
    }
    return 0;
}