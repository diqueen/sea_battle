#include <string>
#include <iostream>
#include "include/Game.hpp"
#include "include/CommandProcessor.hpp"

int main() {
    Game game;
    CommandProcessor processor(game);
    processor.run();
    return 0;
}
