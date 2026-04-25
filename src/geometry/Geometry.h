#pragma once

#include "MeshData.h"
#include <array>

namespace uvc::geom {

// Barycentric point-in-triangle test in UV space.
bool point_in_triangle_barycentric(float px, float py, const std::array<UV, 3>& uvs);

inline BBox compute_bbox(const std::array<UV, 3>& uvs) {
    float minu = uvs[0].u, maxu = uvs[0].u;
    float minv = uvs[0].v, maxv = uvs[0].v;
    for (int i = 1; i < 3; ++i) {
        if (uvs[i].u < minu) minu = uvs[i].u;
        if (uvs[i].u > maxu) maxu = uvs[i].u;
        if (uvs[i].v < minv) minv = uvs[i].v;
        if (uvs[i].v > maxv) maxv = uvs[i].v;
    }
    return {minu, minv, maxu, maxv};
}

} // namespace uvc::geom
