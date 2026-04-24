#include "Geometry.h"
#include <cmath>

namespace uvc::geom {

bool point_in_triangle_barycentric(float px, float py, const std::array<UV, 3>& uvs) {
    const float u0 = uvs[0].u, v0 = uvs[0].v;
    const float u1 = uvs[1].u, v1 = uvs[1].v;
    const float u2 = uvs[2].u, v2 = uvs[2].v;

    const float denom = (v1 - v2) * (u0 - u2) + (u2 - u1) * (v0 - v2);
    if (std::fabs(denom) < 1e-10f) return false;

    const float alpha = ((v1 - v2) * (px - u2) + (u2 - u1) * (py - v2)) / denom;
    const float beta  = ((v2 - v0) * (px - u2) + (u0 - u2) * (py - v2)) / denom;
    const float gamma = 1.0f - alpha - beta;

    constexpr float eps = 1e-6f;
    return alpha >= -eps && beta >= -eps && gamma >= -eps;
}

} // namespace uvc::geom
