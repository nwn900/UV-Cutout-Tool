#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace uvc::geom {

struct UV  { float u = 0.f, v = 0.f; };
struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };
struct BBox { float min_u = 0.f, min_v = 0.f, max_u = 0.f, max_v = 0.f; };

struct Triangle {
    std::array<UV, 3>  uv{};
    std::array<Vec3, 3> verts{};          // optional (NIF only); all-zero for OBJ
    std::array<uint32_t, 3> indices{};    // optional (NIF only)
    float avg_z = 0.f;
    BBox  bbox{};
    bool  selected = false;
    std::optional<int> island_id;         // assigned by IslandBuilder
};

struct Mesh {
    std::string name;
    std::vector<Triangle> triangles;
    bool visible = true;
    std::vector<std::vector<int>> islands; // triangle indices per island
    std::unordered_map<int, bool> island_visible;
};

} // namespace uvc::geom
