#pragma once

#include "MeshData.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace uvc::geom {

class SpatialGrid {
public:
    explicit SpatialGrid(float cell_size = 0.05f);

    void insert(int triangle_idx, const BBox& bbox);
    std::vector<int> query(float u, float v) const;

private:
    struct Key {
        int x, y;
        bool operator==(const Key& o) const noexcept { return x == o.x && y == o.y; }
    };
    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept {
            return (std::size_t(uint32_t(k.x)) * 73856093u) ^ std::size_t(uint32_t(k.y));
        }
    };

    float cell_size_;
    std::unordered_map<Key, std::vector<int>, KeyHash> cells_;
};

} // namespace uvc::geom
