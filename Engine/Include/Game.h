//
// Created by Josh on 5/14/2022.
//

#pragma once
#include <Engine.h>

namespace dragonfire {

class EXPORTED Game {
protected:
    bool running = true;
    /// \brief Implement this function as the main game loop
    /// \param deltaTime the time, in seconds, since the last iteration of the game loop
    virtual void mainLoop(double deltaTime) = 0;

public:
    /// \brief Creates the base game and initializes common subsystems<br>
    /// Child classes should call as part of its constructor's initializer
    /// list to initialize common systems such as logging
    /// \param argc count of the command line arguments
    /// \param argv the command line arguments
    /// \param level the logging level
    Game(int argc, char** argv, spdlog::level::level_enum level = spdlog::level::info);

    /// \brief Runs the game.
    /// Everything starts here
    void run();

    /// Shuts down the game
    virtual ~Game() noexcept;

    Game(Game& other) = delete;
    Game& operator =(Game& other) = delete;
    Game(Game&& other) = delete;
    Game& operator =(Game&& other) = delete;
};

}   // namespace dragonfire
