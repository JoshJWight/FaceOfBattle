#pragma once

#include "core/types.hpp"
#include "core/constants.hpp"
#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>
#include <cmath>

namespace fob {

class SpatialHash {
public:
    explicit SpatialHash(float cellSize = SPATIAL_HASH_CELL_SIZE)
        : m_cellSize(cellSize), m_invCellSize(1.0f / cellSize) {}

    void clear() {
        m_cells.clear();
    }

    void insert(entt::entity entity, float x, float y) {
        auto key = cellKey(x, y);
        m_cells[key].push_back(entity);
    }

    // Query all entities within radius of point
    void queryRadius(float x, float y, float radius,
                     std::vector<entt::entity>& results) const {
        results.clear();

        int minCellX = static_cast<int>(std::floor((x - radius) * m_invCellSize));
        int maxCellX = static_cast<int>(std::floor((x + radius) * m_invCellSize));
        int minCellY = static_cast<int>(std::floor((y - radius) * m_invCellSize));
        int maxCellY = static_cast<int>(std::floor((y + radius) * m_invCellSize));

        for (int cy = minCellY; cy <= maxCellY; ++cy) {
            for (int cx = minCellX; cx <= maxCellX; ++cx) {
                auto key = packKey(cx, cy);
                auto it = m_cells.find(key);
                if (it != m_cells.end()) {
                    results.insert(results.end(), it->second.begin(), it->second.end());
                }
            }
        }
    }

    // Query entities in same cell and neighboring cells (3x3 around point)
    void queryNearby(float x, float y, std::vector<entt::entity>& results) const {
        results.clear();

        int cellX = static_cast<int>(std::floor(x * m_invCellSize));
        int cellY = static_cast<int>(std::floor(y * m_invCellSize));

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                auto key = packKey(cellX + dx, cellY + dy);
                auto it = m_cells.find(key);
                if (it != m_cells.end()) {
                    results.insert(results.end(), it->second.begin(), it->second.end());
                }
            }
        }
    }

    float cellSize() const { return m_cellSize; }

private:
    float m_cellSize;
    float m_invCellSize;
    std::unordered_map<int64_t, std::vector<entt::entity>> m_cells;

    static int64_t packKey(int x, int y) {
        return (static_cast<int64_t>(x) << 32) | (static_cast<uint32_t>(y));
    }

    int64_t cellKey(float x, float y) const {
        int cellX = static_cast<int>(std::floor(x * m_invCellSize));
        int cellY = static_cast<int>(std::floor(y * m_invCellSize));
        return packKey(cellX, cellY);
    }
};

} // namespace fob
