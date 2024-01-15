//
// Created by josh on 8/11/23.
//
#include "crash.h"
#include "spdlog/spdlog.h"
#include <SDL.h>

namespace dragonfire {

#ifdef NDEBUG
void crash(const char* msg)
{
    spdlog::critical("Crash \"{}\"", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_DISPLAY_NAME, msg, nullptr);
    SDL_Quit();
    abort();
}
#else
void crash(const char* msg, const std::source_location& location)
{
    spdlog::critical(
        "Crash \"{}\" at line {} in function {} in file {}",
        msg,
        location.line(),
        location.function_name(),
        location.file_name()
    );
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, APP_DISPLAY_NAME, msg, nullptr);
    SDL_Quit();
    abort();
}
#endif
}// namespace VoidEngine