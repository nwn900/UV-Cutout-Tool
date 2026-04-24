#include "SpatialGrid.h"

#include <cmath>
#include <unordered_set>

namespace uvc::geom {

SpatialGrid::SpatialGrid(float cell_size) : cell_size_(cell_size) {}

void SpatialGrid::insert(int triangle_idx, const BBox& bbox) {
    const int sx = int(std::floor(bbox.min_u / cell_size_));
    const int sy = int(std::floor(bbox.min_v / cell_size_));
    const int ex = int(std::floor(bbox.max_u / cell_size_));
    const int ey = int(std::floor(bbox.max_v / cell_size_));
    for (int gx = sx; gx <= ex; ++gx)
        for (int gy = sy; gy <= ey; ++gy)
            cells_[{gx, gy}].push_back(triangle_idx);
}

std::vector<int> SpatialGrid::query(float u, float v) const {
    const int gx = int(std::floor(u / cell_size_));
    const int gy = int(std::floor(v / cell_size_));
    std::unordered_set<int> cand;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            auto it = cells_.find({gx + dx, gy + dy});
            if (it == cells_.end()) continue;
            cand.insert(it->second.begin(), it->second.end());
        }
    }
    return {cand.begin(), cand.end()};
}

} // namespace uvc::geom
