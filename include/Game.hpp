#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <ostream>
#include <istream>
#include <algorithm>
#include "Ship.hpp"

enum class CellState : uint8_t {
    EMPTY,
    SHIP,
    HIT,
    MISS,
    KILL
};

enum class GameMode : uint8_t {
    MASTER,
    SLAVE
};

enum class ShootResult : uint8_t {
    MISS,
    HIT,
    KILL,
    INVALID
};

enum class Strategy : uint8_t {
    ORDERED,
    CUSTOM
};

inline std::ostream& operator<<(std::ostream& os, const Strategy& strategy) {
    os << static_cast<int>(strategy);
    return os;
}

inline std::istream& operator>>(std::istream& is, Strategy& strategy) {
    int value;
    is >> value;
    strategy = static_cast<Strategy>(value);
    return is;
}

class Game {
private:
    GameMode mode;
    Strategy currentStrategy;
    uint64_t width;
    uint64_t height;
    std::vector<uint8_t> shipCounts;
    std::vector<std::vector<CellState>> myBoard;
    std::vector<std::vector<CellState>> enemyBoard;
    std::vector<Ship> myShips;
    std::vector<Ship> enemyShips;
    std::vector<std::pair<int, int>> myShots;
    std::vector<std::pair<int, int>> enemyShots;
    bool myTurn = true;
    bool gameStarted = false;
    bool gameEnded = false;
    bool placementPhase = true;
    int remainingShips[4] = {1, 2, 3, 4};

    void initializeBoards();
    bool canPlaceShip(uint64_t x, uint64_t y, int size, bool horizontal) const;
    bool isValidPlacement(const Ship& ship) const;
    bool shipsOverlap(const Ship& ship1, const Ship& ship2) const;
    void markAroundShip(const Ship& ship, std::vector<std::vector<CellState>>& board);
    bool canPlaceEnemyShip(uint64_t x, uint64_t y, int size, bool horizontal);
    bool placeEnemyShip(uint64_t x, uint64_t y, int size, bool horizontal);
    bool isValidGameSetup() const;
    bool tryHitShip(uint64_t x, uint64_t y, Ship& ship);
    std::pair<uint64_t, uint64_t> getNextOrderedShot();
    std::pair<uint64_t, uint64_t> getNextCustomShot();

public:
    Game();
    bool createGame(const std::string& mode);
    bool setStrategy(const std::string& strategy);
    bool setWidth(uint64_t w);
    bool setHeight(uint64_t h);
    bool setShipCount(int shipSize, uint64_t count);
    uint64_t getWidth() const;
    uint64_t getHeight() const;
    uint64_t getShipCount(int shipSize) const;
    void resetBoards() { initializeBoards(); }
    bool startGame();
    bool stopGame();
    bool isFinished() const;
    bool isWinner() const;
    bool isLoser() const;
    bool placeShip(int x, int y, int size, bool horizontal);
    bool canPlaceShip(int size) const {
        if (size < 1 || size > 4) return false;
        return remainingShips[size - 1] > 0;
    }
    ShootResult processShot(uint64_t x, uint64_t y);
    std::pair<uint64_t, uint64_t> getNextShot();
    void displayBoards() const;
    void displayEnemyShips() const;
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    void generateRandomShipPlacement();
    bool isCurrentTurn() const { return myTurn; }
    void switchTurn() { myTurn = !myTurn; }
    ShootResult processEnemyShot(uint64_t x, uint64_t y);
    bool isValidPosition(uint64_t x, uint64_t y) const;
    const std::vector<Ship>& getMyShips() const { return myShips; }

    const std::vector<std::vector<CellState>>& getPlayerBoard() const { return myBoard; }
    const std::vector<std::vector<CellState>>& getEnemyBoard() const { return enemyBoard; }
};
