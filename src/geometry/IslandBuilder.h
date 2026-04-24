#pragma once

#include "MeshData.h"
#include <vector>

namespace uvc::geom {

// Populates triangle.island_id and returns a list of islands (each an ordered
// list of triangle indices). Mirrors compute_islands() in the Python source.
std::vector<std::vector<int>> compute_islands(std::vector<Triangle>& triangles);

} // namespace uvc::geom
