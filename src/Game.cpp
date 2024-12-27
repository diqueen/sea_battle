#include "../include/Game.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>

Game::Game() 
    : mode(GameMode::SLAVE)
    , currentStrategy(Strategy::ORDERED)
    , width(0)
    , height(0)
    , shipCounts(4, 0)
    , gameStarted(false)
    , myTurn(false)
{
    initializeBoards();
}

bool Game::createGame(const std::string& mode) {
    //master
    if (mode == "master") {
        this->mode = GameMode::MASTER;
    } else if (mode == "slave") {
        this->mode = GameMode::SLAVE;
    } else {
        return false;
    }

    width = 10;
    height = 10;

    // slave
    if (this->mode == GameMode::SLAVE) {
        shipCounts[0] = 2;  // 2 однопалубных
        shipCounts[1] = 2;  // 2 двухпалубных
        shipCounts[2] = 2;  // 2 трехпалубных
        shipCounts[3] = 1;  // 1 четырехпалубный
    } else {
        std::fill(shipCounts.begin(), shipCounts.end(), 0);
    }

    initializeBoards();
    
    return true;
}

bool Game::setStrategy(const std::string& strategy) {
    if (strategy == "ordered") {
        currentStrategy = Strategy::ORDERED;
        return true;
    } else if (strategy == "custom") {
        currentStrategy = Strategy::CUSTOM;
        return true;
    }
    return false;
}

bool Game::setShipCount(int shipSize, uint64_t count) {
    if (shipSize < 1 || shipSize > 4 || gameStarted) return false;
    
    shipCounts[shipSize - 1] = count;

    myShips.clear();
    enemyShips.clear();
    initializeBoards();
    
    return true;
}

void Game::generateRandomShipPlacement() {
    std::cout << "Starting ship placement..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> disW(0, width - 1);
    std::uniform_int_distribution<uint64_t> disH(0, height - 1);
    std::bernoulli_distribution disDir(0.5);

    myShips.clear();
    enemyShips.clear();
    initializeBoards();

    for (size_t size = 4; size > 0; --size) {
        uint8_t count = shipCounts[size - 1];
        if (count == 0) continue;
        
        int attempts = 0;
        const int MAX_ATTEMPTS = 100; // попытка на оптимизацию

        while (count > 0 && attempts < MAX_ATTEMPTS) {
            uint64_t x = disW(gen);
            uint64_t y = disH(gen);
            bool horizontal = disDir(gen);
            
            if (horizontal && x + size > width) continue;
            if (!horizontal && y + size > height) continue;
            
            if (canPlaceShip(x, y, size, horizontal)) {
                Ship newShip(x, y, size, horizontal);
                if (isValidPlacement(newShip)) {
                    myShips.push_back(newShip);
                    if (horizontal) {
                        for (int i = 0; i < size; ++i) {
                            myBoard[y][x + i] = CellState::SHIP;
                        }
                    } else {
                        for (int i = 0; i < size; ++i) {
                            myBoard[y + i][x] = CellState::SHIP;
                        }
                    }
                    --count;
                    attempts = 0;
                }
            }
            ++attempts;
        }
        
        if (count > 0) {
            myShips.clear();
            enemyShips.clear();
            initializeBoards();
            return;
        }
    }

    for (size_t size = 4; size > 0; --size) {
        uint8_t count = shipCounts[size - 1];
        if (count == 0) continue;
        
        int attempts = 0;
        const int MAX_ATTEMPTS = 100; // тоже попытка

        while (count > 0 && attempts < MAX_ATTEMPTS) {
            uint64_t x = disW(gen);
            uint64_t y = disH(gen);
            bool horizontal = disDir(gen);
            
            if (horizontal && x + size > width) continue;
            if (!horizontal && y + size > height) continue;
            
            if (canPlaceEnemyShip(x, y, size, horizontal)) {
                Ship newShip(x, y, size, horizontal);
                if (isValidPlacement(newShip)) {
                    enemyShips.push_back(newShip);
                    if (horizontal) {
                        for (int i = 0; i < size; ++i) {
                            enemyBoard[y][x + i] = CellState::SHIP;
                        }
                    } else {
                        for (int i = 0; i < size; ++i) {
                            enemyBoard[y + i][x] = CellState::SHIP;
                        }
                    }
                    --count;
                    attempts = 0;
                }
            }
            ++attempts;
        }
        
        if (count > 0) {
            myShips.clear();
            enemyShips.clear();
            initializeBoards();
            return;
        }
    }
}

bool Game::placeShip(int x, int y, int size, bool horizontal) {
    if (!placementPhase || !canPlaceShip(size)) {
        return false;
    }

    Ship newShip(x, y, size, horizontal);
    
    if (!isValidPlacement(newShip)) {
        return false;
    }

    myShips.push_back(newShip);
    remainingShips[size - 1]--;
    
    if (std::all_of(std::begin(remainingShips), std::end(remainingShips), 
                    [](int count) { return count == 0; })) {
        placementPhase = false;
    }

    if (horizontal) {
        for (int i = 0; i < size; ++i) {
            myBoard[y][x + i] = CellState::SHIP;
        }
    } else {
        for (int i = 0; i < size; ++i) {
            myBoard[y + i][x] = CellState::SHIP;
        }
    }

    return true;
}

bool Game::canPlaceShip(uint64_t x, uint64_t y, int size, bool horizontal) const {
    if (horizontal) {
        if (x + size > width) return false;
    } else {
        if (y + size > height) return false;
    }

    for (int i = -1; i <= size; i++) {
        for (int j = -1; j <= 1; j++) {
            uint64_t checkX = horizontal ? x + i : x + j;
            uint64_t checkY = horizontal ? y + j : y + i;
            
            if (checkX >= width || checkY >= height) continue;
            if (checkX == (uint64_t)-1 || checkY == (uint64_t)-1) continue;

            if (myBoard[checkY][checkX] == CellState::SHIP) {
                return false;
            }
        }
    }

    return true;
}

bool Game::tryHitShip(uint64_t x, uint64_t y, Ship& ship) {
    return ship.tryHit(x, y);
}

ShootResult Game::processShot(uint64_t x, uint64_t y) {
    if (!isValidPosition(x, y)) {
        return ShootResult::INVALID;
    }

    if (enemyBoard[y][x] == CellState::SHIP) {
        enemyBoard[y][x] = CellState::HIT;
        
        for (auto& ship : enemyShips) {
            if (ship.containsPosition(x, y)) {
                bool isDestroyed = true;
                for (int i = 0; i < ship.getSize(); ++i) {
                    uint64_t checkX = ship.isHorizontal() ? ship.getX() + i : ship.getX();
                    uint64_t checkY = ship.isHorizontal() ? ship.getY() : ship.getY() + i;
                    if (enemyBoard[checkY][checkX] == CellState::SHIP) {
                        isDestroyed = false;
                        break;
                    }
                }
                
                if (isDestroyed) {
                    for (int i = 0; i < ship.getSize(); ++i) {
                        uint64_t shipX = ship.isHorizontal() ? ship.getX() + i : ship.getX();
                        uint64_t shipY = ship.isHorizontal() ? ship.getY() : ship.getY() + i;
                        enemyBoard[shipY][shipX] = CellState::KILL;
                    }
                    markAroundShip(ship, enemyBoard);
                    return ShootResult::KILL;
                }
                return ShootResult::HIT;
            }
        }
    } else if (enemyBoard[y][x] == CellState::EMPTY) {
        enemyBoard[y][x] = CellState::MISS;
        return ShootResult::MISS;
    }
    
    return ShootResult::INVALID;
}

void Game::markAroundShip(const Ship& ship, std::vector<std::vector<CellState>>& board) {
    int startX = std::max(0, static_cast<int>(ship.getX()) - 1);
    int startY = std::max(0, static_cast<int>(ship.getY()) - 1);
    int shipEndX = static_cast<int>(ship.getX() + (ship.isHorizontal() ? ship.getSize() - 1 : 0));
    int shipEndY = static_cast<int>(ship.getY() + (ship.isHorizontal() ? 0 : ship.getSize() - 1));
    int endX = std::min(static_cast<int>(width - 1), shipEndX + 1);
    int endY = std::min(static_cast<int>(height - 1), shipEndY + 1);

    // вокруг мисс
    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            bool isShipCell = false;
            if (ship.isHorizontal()) {
                isShipCell = (y == ship.getY() && x >= ship.getX() && x <= shipEndX);
            } else {
                isShipCell = (x == ship.getX() && y >= ship.getY() && y <= shipEndY);
            }
            
            if (!isShipCell && board[y][x] == CellState::EMPTY) {
                board[y][x] = CellState::MISS;
            }
        }
    }
}

std::pair<uint64_t, uint64_t> Game::getNextOrderedShot() {
    static uint64_t nextX = 0;
    static uint64_t nextY = 0;

    while (nextY < height) {
        while (nextX < width) {
            if (enemyBoard[nextY][nextX] == CellState::EMPTY) {
                uint64_t x = nextX++;
                return {x, nextY};
            }
            nextX++;
        }
        nextX = 0;
        nextY++;
    }
    return {0, 0};
}

std::pair<uint64_t, uint64_t> Game::getNextCustomShot() {
    static std::vector<std::pair<uint64_t, uint64_t>> lastHits;
    static std::vector<std::vector<bool>> visited(height, std::vector<bool>(width, false));

    if (!lastHits.empty()) {
        auto [lastX, lastY] = lastHits.back();
        
        const std::vector<std::pair<int, int>> directions = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
        for (const auto& [dx, dy] : directions) {
            uint64_t newX = lastX + dx;
            uint64_t newY = lastY + dy;
            
            if (isValidPosition(newX, newY) && !visited[newY][newX]) {
                visited[newY][newX] = true;
                return {newX, newY};
            }
        }
        
        lastHits.pop_back();
        return getNextCustomShot();
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    while (true) {
        uint64_t x = std::uniform_int_distribution<uint64_t>(0, width - 1)(gen);
        uint64_t y = std::uniform_int_distribution<uint64_t>(0, height - 1)(gen);
        
        if ((x + y) % 2 == 0 && !visited[y][x]) {
            visited[y][x] = true;
            if (enemyBoard[y][x] == CellState::HIT) {
                lastHits.push_back({x, y});
            }
            
            return {x, y};
        }
    }
}

std::pair<uint64_t, uint64_t> Game::getNextShot() {
    return (currentStrategy == Strategy::ORDERED) ? 
           getNextOrderedShot() : getNextCustomShot();
}

bool Game::startGame() {
    std::cout << "Starting game..." << std::endl;
    std::cout << "Field size: " << width << "x" << height << std::endl;
    std::cout << "Ship counts: ";
    for (size_t i = 0; i < shipCounts.size(); ++i) {
        std::cout << (i+1) << "-deck: " << shipCounts[i] << ", ";
    }
    std::cout << std::endl;

    if (!isValidGameSetup()) {
        std::cout << "Invalid game setup" << std::endl;
        return false;
    }
    
    // попытки на корабли
    int maxAttempts = 100;
    bool success = false;
    
    std::cout << "Attempting to place ships..." << std::endl;
    while (maxAttempts > 0 && !success) {
        generateRandomShipPlacement();
        if (!myShips.empty() && !enemyShips.empty()) {
            success = true;
            std::cout << "Ships placed successfully" << std::endl;
            break;
        }
        --maxAttempts;
        if (maxAttempts % 10 == 0) {
            std::cout << maxAttempts << " attempts remaining..." << std::endl;
        }
    }
    
    if (!success) {
        std::cout << "Failed to place ships after " << (100 - maxAttempts) << " attempts" << std::endl;
        return false;
    }
    
    gameStarted = true;
    myTurn = (mode == GameMode::SLAVE);

    std::cout << "\nGame started! Available commands:\n";
    std::cout << "- shot x y     : Make a shot at coordinates (x,y)\n";
    std::cout << "- save file    : Save the game to a file\n";
    std::cout << "- load file    : Load the game from a file\n";
    std::cout << "- display      : Show the game boards\n";
    std::cout << "- reveal       : Show enemy ships (debug)\n";
    std::cout << "- stop         : Stop the game\n\n";
    std::cout << "- exit        : Exit the program\n\n";

    return true;
}

bool Game::isValidGameSetup() const {
    // проверка для поля
    if (width == 0 || height == 0) {
        std::cout << "Invalid field size: " << width << "x" << height << std::endl;
        return false;
    }

    // проверка на корабли
    bool hasShips = false;
    for (size_t i = 0; i < shipCounts.size(); ++i) {
        if (shipCounts[i] > 0) {
            hasShips = true;
            break;
        }
    }
    if (!hasShips) {
        std::cout << "No ships defined" << std::endl;
        return false;
    }

    // проверка на биг корабль
    int maxShipSize = 0;
    for (size_t i = 0; i < shipCounts.size(); ++i) {
        if (shipCounts[i] > 0) {
            maxShipSize = i + 1;
        }
    }
    if (maxShipSize > width || maxShipSize > height) {
        std::cout << "Largest ship size " << maxShipSize << " doesn't fit on " << width << "x" << height << " field" << std::endl;
        return false;
    }

    // S кор
    int totalShipCells = 0;
    for (size_t i = 0; i < shipCounts.size(); ++i) {
        totalShipCells += (i + 1) * shipCounts[i];
    }

    // Sк < Sп
    if (totalShipCells > (width * height) / 2) {
        std::cout << "Total ship cells " << totalShipCells << " is too large for field area " << width * height << std::endl;
        return false;
    }

    return true;
}

uint64_t Game::getWidth() const {
    return width;
}

uint64_t Game::getHeight() const {
    return height;
}

uint64_t Game::getShipCount(int shipSize) const {
    if (shipSize < 1 || shipSize > 4) return 0;
    return shipCounts[shipSize - 1];
}

bool Game::setWidth(uint64_t w) {
    if (gameStarted || w > 100) return false;
    width = w;
    initializeBoards();
    return true;
}

bool Game::setHeight(uint64_t h) {
    if (gameStarted || h > 100) return false;
    height = h;
    initializeBoards();
    return true;
}

void Game::initializeBoards() {
    myBoard.clear();
    enemyBoard.clear();
    
    if (width == 0 || height == 0) return;
    
    // доски
    myBoard = std::vector<std::vector<CellState>>(height, std::vector<CellState>(width, CellState::EMPTY));
    enemyBoard = std::vector<std::vector<CellState>>(height, std::vector<CellState>(width, CellState::EMPTY));
}

void Game::displayBoards() const {
    std::cout << "\nGame Mode: " << (mode == GameMode::MASTER ? "Master" : "Slave") << std::endl;
    if (gameStarted) {
        std::cout << "Current Turn: " << (myTurn ? "Your Turn" : "Enemy's Turn") << std::endl;
    }
    std::cout << std::endl;

    auto printColumnNumbers = [this]() {
        std::cout << "     ";
        for (uint64_t x = 0; x < width; ++x) {
            std::cout << x << "   ";
        }
        std::cout << std::endl;
    };

    std::cout << "\nMy Board:" << std::endl;
    printColumnNumbers();

    for (uint64_t y = 0; y < height; ++y) {
        std::cout << y << "   ";
        for (uint64_t x = 0; x < width; ++x) {
            char symbol = ' ';
            switch (myBoard[y][x]) {
                case CellState::EMPTY: symbol = '.'; break;
                case CellState::SHIP: {
                    for (const auto& ship : myShips) {
                        if (ship.containsPosition(x, y)) {
                            symbol = '0' + ship.getSize();
                            break;
                        }
                    }
                    break;
                }
                case CellState::HIT: symbol = 'X'; break;
                case CellState::MISS: symbol = 'O'; break;
            }
            std::cout << symbol << "   ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Enemy Board:" << std::endl;
    printColumnNumbers();

    for (uint64_t y = 0; y < height; ++y) {
        std::cout << y << "   ";
        for (uint64_t x = 0; x < width; ++x) {
            char symbol = ' ';
            switch (enemyBoard[y][x]) {
                case CellState::EMPTY: symbol = '.'; break;
                case CellState::SHIP: symbol = '.'; break;
                case CellState::HIT: symbol = 'X'; break;
                case CellState::MISS: symbol = 'O'; break;
            }
            std::cout << symbol << "   ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void Game::displayEnemyShips() const {
    std::cout << "\nEnemy ships positions:\n";
    
    std::vector<std::vector<char>> tempBoard(height, std::vector<char>(width, '.'));

    for (const auto& ship : enemyShips) {
        int x = ship.getX();
        int y = ship.getY();
        bool isHorizontal = ship.isHorizontal();
        int size = ship.getSize();
        
        for (int i = 0; i < size; ++i) {
            if (isHorizontal) {
                tempBoard[y][x + i] = 'S';
            } else {
                tempBoard[y + i][x] = 'S';
            }
        }
    }
    
    std::cout << "  ";
    for (size_t i = 0; i < width; ++i) {
        std::cout << i << " ";
    }
    std::cout << "\n";
    
    for (size_t i = 0; i < height; ++i) {
        std::cout << i << " ";
        for (size_t j = 0; j < width; ++j) {
            std::cout << tempBoard[i][j] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

bool Game::stopGame() {
    gameStarted = false;
    return true;
}

bool Game::isValidPosition(uint64_t x, uint64_t y) const {
    return x < width && y < height;
}

bool Game::saveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file) return false;

    file << width << " " << height << "\n";
    file << currentStrategy << "\n";
    file << gameStarted << "\n";
    file << myTurn << "\n";

    file << myShips.size() << "\n";
    for (const auto& ship : myShips) {
        file << ship.getSize() << " " << ship.getX() << " " << ship.getY() << " "
             << (ship.isHorizontal() ? "1" : "0") << "\n";
    }

    for (uint64_t i = 0; i < height; ++i) {
        for (uint64_t j = 0; j < width; ++j) {
            file << static_cast<int>(myBoard[i][j]) << " ";
        }
        file << "\n";
    }

    for (uint64_t i = 0; i < height; ++i) {
        for (uint64_t j = 0; j < width; ++j) {
            file << static_cast<int>(enemyBoard[i][j]) << " ";
        }
        file << "\n";
    }

    return true;
}

bool Game::loadFromFile(const std::string& path) {
    if (gameStarted) return false;

    std::ifstream file(path);
    if (!file) return false;

    uint64_t w, h;
    file >> w >> h;
    
    if (w == 0 || h == 0) return false;

    myShips.clear();
    setWidth(w);
    setHeight(h);

    int size;
    uint64_t x, y;
    std::string horizontal;
    
    while (file >> size >> x >> y >> horizontal) {
        if (size < 1 || size > 4) continue;
        bool isHorizontal = (horizontal == "1");
        placeShip(x, y, size, isHorizontal);
    }
    
    return true;
}

bool Game::isFinished() const {
    // проверка на кол-во кораблей противника (победа)
    for (const auto& ship : enemyShips) {
        bool allHit = true;
        for (int i = 0; i < ship.getSize(); i++) {
            uint64_t x = ship.isHorizontal() ? ship.getX() + i : ship.getX();
            uint64_t y = ship.isHorizontal() ? ship.getY() : ship.getY() + i;
            if (enemyBoard[y][x] != CellState::HIT) {
                allHit = false;
                break;
            }
        }
        if (!allHit) return false;
    }
    return true;
}

bool Game::isWinner() const {
    if (!isFinished()) return false;
    
    // вторая проверка
    for (const auto& ship : enemyShips) {
        bool allHit = true;
        for (int i = 0; i < ship.getSize(); i++) {
            uint64_t x = ship.isHorizontal() ? ship.getX() + i : ship.getX();
            uint64_t y = ship.isHorizontal() ? ship.getY() : ship.getY() + i;
            if (enemyBoard[y][x] != CellState::HIT) {
                allHit = false;
                break;
            }
        }
        if (!allHit) return false;
    }
    return true;
}

bool Game::isLoser() const {
    if (!isFinished()) return false;
    
    // проверка на наши корабли (поражение)
    for (const auto& ship : myShips) {
        bool allHit = true;
        for (int i = 0; i < ship.getSize(); i++) {
            uint64_t x = ship.isHorizontal() ? ship.getX() + i : ship.getX();
            uint64_t y = ship.isHorizontal() ? ship.getY() : ship.getY() + i;
            if (myBoard[y][x] != CellState::HIT) {
                allHit = false;
                break;
            }
        }
        if (!allHit) return false;
    }
    return true;
}

bool Game::canPlaceEnemyShip(uint64_t x, uint64_t y, int size, bool horizontal) {
    if (horizontal) {
        if (x + size > width) return false;
    } else {
        if (y + size > height) return false;
    }

    for (int i = -1; i <= size; i++) {
        for (int j = -1; j <= 1; j++) {
            uint64_t checkX = horizontal ? x + i : x + j;
            uint64_t checkY = horizontal ? y + j : y + i;

            if (checkX >= width || checkY >= height) continue;
            if (checkX == (uint64_t)-1 || checkY == (uint64_t)-1) continue;

            if (enemyBoard[checkY][checkX] == CellState::SHIP) {
                return false;
            }
        }
    }
    return true;
}

bool Game::placeEnemyShip(uint64_t x, uint64_t y, int size, bool horizontal) {
    if (!canPlaceEnemyShip(x, y, size, horizontal)) {
        return false;
    }

    Ship newShip(x, y, size, horizontal);
    enemyShips.push_back(newShip);

    for (int i = 0; i < size; ++i) {
        if (horizontal) {
            enemyBoard[y][x + i] = CellState::SHIP;
        } else {
            enemyBoard[y + i][x] = CellState::SHIP;
        }
    }
    return true;
}

ShootResult Game::processEnemyShot(uint64_t x, uint64_t y) {
    if (!isValidPosition(x, y)) {
        return ShootResult::INVALID;
    }

    // молния
    if (myBoard[y][x] == CellState::HIT || 
        myBoard[y][x] == CellState::KILL ||
        myBoard[y][x] == CellState::MISS) {
        return ShootResult::INVALID;
    }

    bool isHit = false;
    bool isDestroyed = false;

    // хит мисс
    for (auto& ship : myShips) {
        if (ship.containsPosition(x, y)) {
            isHit = true;
            myBoard[y][x] = CellState::HIT;
            
            if (ship.tryHit(x, y)) {
                if (ship.isDestroyed()) {
                    isDestroyed = true;
                    markAroundShip(ship, myBoard);
                }
            }
            break;
        }
    }

    if (!isHit) {
        myBoard[y][x] = CellState::MISS;
        return ShootResult::MISS;
    }

    return isDestroyed ? ShootResult::KILL : ShootResult::HIT;
}

bool Game::isValidPlacement(const Ship& ship) const {
    if (ship.getX() < 0 || ship.getY() < 0) return false;
    
    if (ship.isHorizontal()) {
        if (ship.getX() + ship.getSize() > width) return false;
        if (ship.getY() >= height) return false;
    } else {
        if (ship.getY() + ship.getSize() > height) return false;
        if (ship.getX() >= width) return false;
    }

    // проверка пересеч
    for (const auto& existingShip : myShips) {
        if (shipsOverlap(existingShip, ship)) {
            return false;
        }
    }

    return true;
}

bool Game::shipsOverlap(const Ship& ship1, const Ship& ship2) const {
    auto cells1 = ship1.getOccupiedCells();
    auto cells2 = ship2.getOccupiedCells();

    for (const auto& cell1 : cells1) {
        for (const auto& cell2 : cells2) {
            if (cell1.first == cell2.first && cell1.second == cell2.second) {
                return true;
            }
        }
    }
    return false;
}
