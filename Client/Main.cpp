//
// Created by Josh on 5/14/2022.
//
#include <GameClient.h>
#include <SDL.h>
#include <SDL_main.h>

using namespace dragonfire;

int main(int argc, char** argv) {
    GameClient game(argc, argv);
    game.run();
    return 0;
}