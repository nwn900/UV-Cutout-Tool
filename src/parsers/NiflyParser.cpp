#include "NiflyParser.h"
#include "../geometry/Geometry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace uvc::parsers {

using namespace uvc::geom;

struct NiflyParser::Impl {
#ifdef _WIN32
    HMODULE module = nullptr;
#else
    void* module = nullptr;
#endif

    using load_fn       = void* (*)(const char*);
    using destroy_fn    = void  (*)(void*);
    using getShapes_fn  = int   (*)(void* file, void** out, int max, int flags);
    using getVerts_fn   = int   (*)(void* file, void* shape, float* out, int max, int flags);
    using getUVs_fn     = int   (*)(void* file, void* shape, float* out, int max, int flags);
    using getTris_fn    = int   (*)(void* file, void* shape, uint16_t* out, int max, int flags);
    using getMsg_fn     = int   (*)(char* out, int max);

    load_fn      nifly_load      = nullptr;
    destroy_fn   nifly_destroy   = nullptr;
    getShapes_fn nifly_getShapes = nullptr;
    getVerts_fn  nifly_getVerts  = nullptr;
    getUVs_fn    nifly_getUVs    = nullptr;
    getTris_fn   nifly_getTris   = nullptr;
    getMsg_fn    nifly_getMsg    = nullptr;

    ~Impl() { unload(); }

    void unload() {
#ifdef _WIN32
        if (module) { FreeLibrary(module); module = nullptr; }
#else
        if (module) { dlclose(module); module = nullptr; }
#endif
    }

    template <typename Fn>
    Fn resolve(const char* name) {
#ifdef _WIN32
        return reinterpret_cast<Fn>(GetProcAddress(module, name));
#else
        return reinterpret_cast<Fn>(dlsym(module, name));
#endif
    }

    bool load_library(const QString& dll_path) {
#ifdef _WIN32
        std::wstring w = dll_path.toStdWString();
        module = LoadLibraryExW(
            w.c_str(),
            nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
        module = dlopen(dll_path.toUtf8().constData(), RTLD_NOW);
#endif
        if (!module) return false;
        nifly_load      = resolve<load_fn>("load");
        nifly_destroy   = resolve<destroy_fn>("destroy");
        nifly_getShapes = resolve<getShapes_fn>("getShapes");
        nifly_getVerts  = resolve<getVerts_fn>("getVertsForShape");
        nifly_getUVs    = resolve<getUVs_fn>("getUVs");
        nifly_getTris   = resolve<getTris_fn>("getTriangles");
        nifly_getMsg    = resolve<getMsg_fn>("getMessageLog");
        const bool ok =
            nifly_load && nifly_destroy && nifly_getShapes &&
            nifly_getVerts && nifly_getUVs && nifly_getTris && nifly_getMsg;
        if (!ok) unload();
        return ok;
    }

    QString last_message() {
        if (!nifly_getMsg) return {};
        int size = nifly_getMsg(nullptr, 0) + 2;
        if (size <= 2) return {};
        std::vector<char> buf(size, 0);
        nifly_getMsg(buf.data(), size);
        return QString::fromUtf8(buf.data());
    }
};

NiflyParser::NiflyParser() : d_(std::make_unique<Impl>()) {}
NiflyParser::~NiflyParser() = default;

static QString validated_dll_path(const QString& candidate) {
    const QFileInfo info(candidate);
    if (!info.exists() || !info.isFile()) return {};
    if (info.fileName().compare("NiflyDLL.dll", Qt::CaseInsensitive) != 0) return {};
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QString() : QDir::cleanPath(canonical);
}

static QStringList candidate_dll_paths() {
    QStringList out;
    const QString exe = QDir::cleanPath(QCoreApplication::applicationDirPath());

    // Portable app-folder layout first.
    out << QDir(exe).filePath("NiflyDLL.dll");
    out << QDir(exe).filePath("pynifly_lib/io_scene_nifly/pyn/NiflyDLL.dll");

    // Development fallbacks.
    out << QDir(exe).filePath("../pynifly_lib/io_scene_nifly/pyn/NiflyDLL.dll");
    out << QDir(exe).filePath("../../pynifly_lib/io_scene_nifly/pyn/NiflyDLL.dll");
    return out;
}

std::vector<Mesh> NiflyParser::parse(const QString& filepath, ProgressCallback cb) {
    if (!d_->module) {
        QString found;
        for (const QString& p : candidate_dll_paths()) {
            const QString validated = validated_dll_path(p);
            if (!validated.isEmpty()) { found = validated; break; }
        }
        if (found.isEmpty())
            throw std::runtime_error("NiflyDLL.dll not found in the app folder or supported dev paths");
        if (!d_->load_library(found))
            throw std::runtime_error(("Failed to load NiflyDLL from " + found).toStdString());
    }

    if (cb) cb(0.1f, "Loading NIF...");

    QByteArray utf8 = filepath.toUtf8();
    void* handle = d_->nifly_load(utf8.constData());
    if (!handle) {
        QString msg = d_->last_message();
        throw std::runtime_error(("Failed to load NIF: " + msg).toStdString());
    }

    if (cb) cb(0.2f, "Reading shapes...");

    int n_shapes = d_->nifly_getShapes(handle, nullptr, 0, 0);
    if (n_shapes < 0) {
        d_->nifly_destroy(handle);
        throw std::runtime_error("NiflyDLL returned an invalid shape count");
    }
    if (n_shapes == 0) {
        d_->nifly_destroy(handle);
        return {};
    }
    if (n_shapes > 4096) {
        d_->nifly_destroy(handle);
        throw std::runtime_error("NIF contains an unreasonable number of shapes");
    }

    std::vector<void*> shape_handles(size_t(n_shapes), nullptr);
    const int resolved_shapes = d_->nifly_getShapes(handle, shape_handles.data(), n_shapes, 0);
    if (resolved_shapes < 0 || resolved_shapes > n_shapes) {
        d_->nifly_destroy(handle);
        throw std::runtime_error("NiflyDLL returned inconsistent shape data");
    }

    constexpr int MAX_VERTS = 100000;
    constexpr int MAX_TRIS = 100000;

    std::vector<Mesh> meshes;

    std::vector<float>    uv_buf(MAX_VERTS * 2);
    std::vector<float>    vert_buf(MAX_VERTS * 3);
    std::vector<uint16_t> tri_buf(MAX_VERTS * 3);

    for (int i = 0; i < resolved_shapes; ++i) {
        if (cb) cb(0.3f + 0.6f * (float(i) / float(std::max(resolved_shapes, 1))), "Processing shape...");

        int n_uvs  = d_->nifly_getUVs  (handle, shape_handles[i], uv_buf.data(),  int(uv_buf.size()),  0);
        int n_vs   = d_->nifly_getVerts(handle, shape_handles[i], vert_buf.data(),int(vert_buf.size()),0);
        int n_tris = d_->nifly_getTris (handle, shape_handles[i], tri_buf.data(), int(tri_buf.size()), 0);

        if (n_uvs < 0 || n_vs < 0 || n_tris < 0) {
            d_->nifly_destroy(handle);
            throw std::runtime_error("NiflyDLL returned a negative count");
        }
        if (n_uvs > MAX_VERTS || n_vs > MAX_VERTS || n_tris > MAX_TRIS) {
            d_->nifly_destroy(handle);
            throw std::runtime_error("NIF shape exceeds supported limits");
        }
        if (n_uvs <= 0 || n_vs <= 0 || n_tris <= 0) continue;

        Mesh m;
        m.name = std::string("Shape_") + std::to_string(i + 1);
        m.visible = true;
        m.triangles.reserve(n_tris);

        for (int j = 0; j < n_tris; ++j) {
            uint16_t a = tri_buf[j * 3 + 0];
            uint16_t b = tri_buf[j * 3 + 1];
            uint16_t c = tri_buf[j * 3 + 2];
            if (a >= uint16_t(n_uvs) || b >= uint16_t(n_uvs) || c >= uint16_t(n_uvs)) continue;
            if (a >= uint16_t(n_vs)  || b >= uint16_t(n_vs)  || c >= uint16_t(n_vs)) continue;

            Triangle t{};
            t.indices = {a, b, c};
            std::array<uint16_t, 3> idx{a, b, c};
            for (int k = 0; k < 3; ++k) {
                uint16_t id = idx[k];
                t.uv[k] = { uv_buf[id * 2 + 0], uv_buf[id * 2 + 1] };
                t.verts[k] = { vert_buf[id * 3 + 0], vert_buf[id * 3 + 1], vert_buf[id * 3 + 2] };
            }
            t.bbox  = compute_bbox(t.uv);
            t.avg_z = (t.verts[0].z + t.verts[1].z + t.verts[2].z) / 3.0f;
            m.triangles.push_back(std::move(t));
        }

        if (!m.triangles.empty()) meshes.push_back(std::move(m));
    }

    d_->nifly_destroy(handle);
    if (cb) cb(1.0f, "Loaded mesh");
    return meshes;
}

} // namespace uvc::parsers
