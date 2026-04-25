#pragma once

#include "MeshData.h"
#include <vector>

namespace uvc::geom {

// Populates triangle.island_id and returns each UV island as an ordered list of
// triangle indices.
std::vector<std::vector<int>> compute_islands(std::vector<Triangle>& triangles);

} // namespace uvc::geom
