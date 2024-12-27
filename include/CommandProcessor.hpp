#pragma once
#include "Game.hpp"
#include <string>

class CommandProcessor {
private:
    Game& game;
    std::string makeEnemyShot();
    std::pair<std::string, std::string> parseCommand(const std::string& command);

public:
    CommandProcessor(Game& game);
    void run();
    std::string processCommand(const std::string& command);
};
