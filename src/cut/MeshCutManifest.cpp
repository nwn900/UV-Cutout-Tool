#include "MeshCutManifest.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>

namespace uvc::cut {
namespace {

using uvc::geom::Mesh;
using uvc::geom::Triangle;

QByteArray bytesOf(const void* ptr, int size) {
    return QByteArray::fromRawData(reinterpret_cast<const char*>(ptr), size);
}

template <typename T>
void addScalar(QCryptographicHash& hash, const T& value) {
    hash.addData(bytesOf(&value, int(sizeof(T))));
}

QString sha256File(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) hash.addData(f.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

QString shapeSignature(const Mesh& mesh) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto& tri : mesh.triangles) {
        for (uint32_t idx : tri.indices) addScalar(hash, idx);
        for (const auto& uv : tri.uv) {
            addScalar(hash, uv.u);
            addScalar(hash, uv.v);
        }
        for (const auto& v : tri.verts) {
            addScalar(hash, v.x);
            addScalar(hash, v.y);
            addScalar(hash, v.z);
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString commonSourceRoot(const std::vector<Mesh>& meshes) {
    QStringList parts;
    bool initialized = false;
    for (const auto& mesh : meshes) {
        if (mesh.source_path.empty()) continue;
        const QString dir = QDir::cleanPath(QFileInfo(QString::fromStdString(mesh.source_path)).absolutePath());
        QStringList current = QDir::fromNativeSeparators(dir).split('/', Qt::SkipEmptyParts);
        if (!initialized) {
            parts = current;
            initialized = true;
            continue;
        }
        int shared = 0;
        while (shared < parts.size() && shared < current.size() && parts[shared].compare(current[shared], Qt::CaseInsensitive) == 0)
            ++shared;
        parts = parts.mid(0, shared);
    }
    if (parts.isEmpty()) return {};
    QString root = parts.join('/');
    if (parts.first().endsWith(':')) {
        root = parts.first() + '/' + parts.mid(1).join('/');
    }
    return QDir::cleanPath(root);
}

QJsonArray triArray(const Triangle& tri) {
    QJsonArray tri_json;
    for (const auto& uv : tri.uv) {
        QJsonArray uv_json;
        uv_json.append(uv.u);
        uv_json.append(uv.v);
        tri_json.append(uv_json);
    }
    return tri_json;
}

QJsonArray vec3Array(const Triangle& tri) {
    QJsonArray verts_json;
    for (const auto& v : tri.verts) {
        QJsonArray v_json;
        v_json.append(v.x);
        v_json.append(v.y);
        v_json.append(v.z);
        verts_json.append(v_json);
    }
    return verts_json;
}

QJsonObject meshObject(const Mesh& mesh, int mesh_index,
                       const QString& selection_mode) {
    QJsonObject mesh_json;
    const QString source_path = QString::fromStdString(mesh.source_path);
    const QString source_name = !source_path.isEmpty()
        ? QFileInfo(source_path).fileName()
        : QString::fromStdString(mesh.source_name);

    int max_vert_index = -1;
    int selected_count = 0;
    QJsonArray tris_json;
    for (int ti = 0; ti < int(mesh.triangles.size()); ++ti) {
        const auto& tri = mesh.triangles[ti];
        for (uint32_t idx : tri.indices)
            max_vert_index = std::max(max_vert_index, int(idx));
        if (!tri.selected) continue;
        ++selected_count;

        QJsonObject tri_json;
        tri_json["triangleIndex"] = ti;

        QJsonArray indices_json;
        for (uint32_t idx : tri.indices) indices_json.append(int(idx));
        tri_json["indices"] = indices_json;
        tri_json["uv"] = triArray(tri);
        tri_json["verts"] = vec3Array(tri);
        tri_json["avgZ"] = tri.avg_z;
        tris_json.append(tri_json);
    }

    mesh_json["meshIndex"] = mesh_index;
    mesh_json["sourcePath"] = QDir::fromNativeSeparators(source_path);
    mesh_json["sourceName"] = source_name;
    mesh_json["shapeName"] = QString::fromStdString(mesh.name);
    mesh_json["inputSha256"] = sha256File(source_path);
    mesh_json["shapeSha256"] = shapeSignature(mesh);
    mesh_json["vertexCount"] = std::max(max_vert_index + 1, 0);
    mesh_json["triangleCount"] = int(mesh.triangles.size());
    mesh_json["selectedTriangleCount"] = selected_count;
    mesh_json["selectionMode"] = selection_mode;
    mesh_json["triangles"] = tris_json;
    return mesh_json;
}

} // namespace

QJsonDocument buildMeshCutManifest(const std::vector<Mesh>& meshes,
                                   const QString& selection_mode) {
    QJsonArray mesh_entries;
    for (int i = 0; i < int(meshes.size()); ++i) {
        const auto& mesh = meshes[i];
        if (std::none_of(mesh.triangles.begin(), mesh.triangles.end(),
                         [](const Triangle& t) { return t.selected; })) {
            continue;
        }
        mesh_entries.append(meshObject(mesh, i, selection_mode));
    }

    if (mesh_entries.isEmpty()) return QJsonDocument();

    QJsonObject root;
    root["schema"] = "uv-cutout-tool.mesh-cut-manifest.v1";
    root["sourceRoot"] = commonSourceRoot(meshes);
    root["selectionMode"] = selection_mode;
    root["meshCount"] = int(mesh_entries.size());
    root["meshes"] = mesh_entries;
    return QJsonDocument(root);
}

bool writeMeshCutManifest(const QString& path,
                          const std::vector<Mesh>& meshes,
                          const QString& selection_mode,
                          QString* error) {
    const QJsonDocument doc = buildMeshCutManifest(meshes, selection_mode);
    if (doc.isNull()) {
        if (error) *error = QStringLiteral("No selected triangles were found.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("Could not open manifest for writing.");
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = QStringLiteral("Could not finalize the manifest file.");
        return false;
    }
    return true;
}

} // namespace uvc::cut
