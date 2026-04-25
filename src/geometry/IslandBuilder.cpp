#include "IslandBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace uvc::geom {

namespace {
struct UVKey {
    int32_t qu, qv;
    bool operator==(const UVKey& o) const noexcept { return qu == o.qu && qv == o.qv; }
};
struct UVKeyHash {
    std::size_t operator()(const UVKey& k) const noexcept {
        return (std::size_t(uint32_t(k.qu)) * 2654435761u) ^ std::size_t(uint32_t(k.qv));
    }
};
}

std::vector<std::vector<int>> compute_islands(std::vector<Triangle>& triangles) {
    std::vector<std::vector<int>> islands;
    if (triangles.empty()) return islands;

    // Quantize UVs to three decimal places before linking shared edges.
    constexpr float kScale = 1000.0f;
    auto quantize = [](float x) -> int32_t {
        return int32_t(std::lround(double(x) * double(kScale)));
    };

    std::unordered_map<UVKey, std::vector<int>, UVKeyHash> uv_vertex_map;
    uv_vertex_map.reserve(triangles.size() * 3);
    for (size_t i = 0; i < triangles.size(); ++i) {
        for (const auto& uv : triangles[i].uv) {
            UVKey k{quantize(uv.u), quantize(uv.v)};
            uv_vertex_map[k].push_back(int(i));
        }
    }

    std::vector<std::unordered_set<int>> adj(triangles.size());
    for (auto& kv : uv_vertex_map) {
        auto& list = kv.second;
        // Dedup triangle indices per uv vertex (one triangle contributes at most 3 times here).
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
        for (size_t i = 0; i < list.size(); ++i) {
            for (size_t j = i + 1; j < list.size(); ++j) {
                adj[list[i]].insert(list[j]);
                adj[list[j]].insert(list[i]);
            }
        }
    }

    std::vector<uint8_t> visited(triangles.size(), 0);
    for (size_t start = 0; start < triangles.size(); ++start) {
        if (visited[start]) continue;
        std::vector<int> island;
        std::vector<int> stack = {int(start)};
        while (!stack.empty()) {
            int node = stack.back();
            stack.pop_back();
            if (visited[node]) continue;
            visited[node] = 1;
            island.push_back(node);
            for (int n : adj[node])
                if (!visited[n]) stack.push_back(n);
        }
        if (!island.empty()) {
            std::sort(island.begin(), island.end());
            islands.push_back(std::move(island));
        }
    }

    for (int island_id = 0; island_id < int(islands.size()); ++island_id) {
        for (int tri_idx : islands[island_id])
            triangles[tri_idx].island_id = island_id;
    }

    return islands;
}

} // namespace uvc::geom
