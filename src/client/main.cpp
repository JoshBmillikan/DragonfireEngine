//
// Created by josh on 1/13/24.
//

#include "app.h"
#include "core/crash.h"

#include <SDL2/SDL_main.h>

extern "C" int main(const int argc, char** argv)
{
    dragonfire::crashOnException([&] {
        dragonfire::App app(argc, argv);
        app.run();
    });
    return 0;
}
