#pragma once
#include <cstdint>
#include <vector>

class Ship {
private:
    uint64_t x;
    uint64_t y;
    uint8_t size;
    bool horizontal;
    std::vector<bool> hits;

public:
    Ship(uint64_t x, uint64_t y, uint8_t size, bool horizontal)
        : x(x), y(y), size(size), horizontal(horizontal), hits(size, false) {}

    bool containsPosition(uint64_t posX, uint64_t posY) const {
        if (horizontal) {
            return posY == y && posX >= x && posX < x + size;
        } else {
            return posX == x && posY >= y && posY < y + size;
        }
    }

    uint64_t getX() const { return x; }
    uint64_t getY() const { return y; }
    uint8_t getSize() const { return size; }
    bool isHorizontal() const { return horizontal; }
    
    bool tryHit(uint64_t posX, uint64_t posY) {
        if (!containsPosition(posX, posY)) return false;
        int index = horizontal ? posX - x : posY - y;
        if (hits[index]) return false;
        hits[index] = true;
        return true;
    }

    bool isDestroyed() const {
        for (bool hit : hits) {
            if (!hit) return false;
        }
        return true;
    }

    std::vector<std::pair<int, int>> getOccupiedCells() const {
        std::vector<std::pair<int, int>> cells;
        for (int i = 0; i < size; ++i) {
            if (horizontal) {
                cells.push_back({x + i, y});
            } else {
                cells.push_back({x, y + i});
            }
        }
        return cells;
    }
};
