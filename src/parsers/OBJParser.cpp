#include "OBJParser.h"
#include "../geometry/Geometry.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <array>
#include <unordered_map>
#include <cstdlib>

namespace uvc::parsers {

using namespace uvc::geom;

namespace {

struct FaceVert { int vert_idx; int uv_idx; };

struct State {
    std::vector<Vec3> vertices;
    std::vector<UV>   uv_coords;
    std::vector<std::vector<FaceVert>> faces;
    std::string current_group = "default";
    // Preserve insertion order of groups (Python's dict-in-Python-3.7 behavior).
    std::vector<std::string> group_order;
    std::unordered_map<std::string, std::vector<int>> group_faces; // indices into faces
};

void parse_face(State& st, const QStringList& parts) {
    std::vector<FaceVert> face_verts;
    face_verts.reserve(parts.size());
    for (const QString& p : parts) {
        auto pieces = p.split('/');
        int vi = 0, ui = 0;
        if (!pieces.isEmpty() && !pieces[0].isEmpty()) vi = pieces[0].toInt() - 1;
        if (pieces.size() > 1 && !pieces[1].isEmpty()) ui = pieces[1].toInt() - 1;
        face_verts.push_back({vi, ui});
    }
    st.faces.push_back(std::move(face_verts));
    if (st.group_faces.find(st.current_group) == st.group_faces.end())
        st.group_order.push_back(st.current_group);
    st.group_faces[st.current_group].push_back(int(st.faces.size()) - 1);
}

std::vector<Mesh> build_meshes(State& st) {
    std::vector<Mesh> meshes;

    auto build_from_face_indices = [&](const std::string& name, const std::vector<int>& face_idx_list) -> Mesh {
        Mesh m;
        m.name = name;
        m.visible = true;
        for (int fi : face_idx_list) {
            const auto& face = st.faces[fi];
            if (face.size() < 3) continue;
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                std::array<UV, 3> tri_uvs{};
                int idxs[3] = {0, int(i), int(i + 1)};
                for (int j = 0; j < 3; ++j) {
                    const auto& fv = face[idxs[j]];
                    if (fv.uv_idx >= 0 && fv.uv_idx < int(st.uv_coords.size()))
                        tri_uvs[j] = st.uv_coords[fv.uv_idx];
                }
                Triangle t{};
                t.uv = tri_uvs;
                t.bbox = compute_bbox(tri_uvs);
                m.triangles.push_back(std::move(t));
            }
        }
        return m;
    };

    for (const auto& name : st.group_order) {
        const auto& idxs = st.group_faces[name];
        if (idxs.empty()) continue;
        Mesh m = build_from_face_indices(name, idxs);
        if (!m.triangles.empty()) meshes.push_back(std::move(m));
    }

    if (meshes.empty() && !st.faces.empty()) {
        std::vector<int> all(st.faces.size());
        for (size_t i = 0; i < all.size(); ++i) all[i] = int(i);
        Mesh m = build_from_face_indices("default", all);
        if (!m.triangles.empty()) meshes.push_back(std::move(m));
    }

    return meshes;
}

} // namespace

std::vector<Mesh> OBJParser::parse(const QString& filepath) {
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);

    State st;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        const QString cmd = parts[0];

        if (cmd == "v") {
            if (parts.size() >= 4) {
                bool okx = false, oky = false, okz = false;
                float x = parts[1].toFloat(&okx);
                float y = parts[2].toFloat(&oky);
                float z = parts[3].toFloat(&okz);
                if (okx && oky && okz) st.vertices.push_back({x, y, z});
            }
        }
        else if (cmd == "vt") {
            if (parts.size() >= 2) {
                bool oku = false, okv = false;
                float u = parts[1].toFloat(&oku);
                float v = (parts.size() >= 3) ? parts[2].toFloat(&okv) : 0.0f;
                if (oku) st.uv_coords.push_back({u, v});
            }
        }
        else if (cmd == "f") {
            parts.removeFirst();
            if (parts.size() >= 3) parse_face(st, parts);
        }
        else if (cmd == "g" || cmd == "o") {
            parts.removeFirst();
            st.current_group = parts.isEmpty()
                ? (std::string("group_") + std::to_string(st.group_order.size()))
                : parts.join(' ').toStdString();
        }
    }

    return build_meshes(st);
}

} // namespace uvc::parsers
