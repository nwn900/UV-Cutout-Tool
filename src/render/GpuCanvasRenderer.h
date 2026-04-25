#pragma once

#include "CanvasRenderer.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <memory>

namespace uvc::render {

// GPU-driven renderer: all wireframe lines become a single glDrawArrays(GL_LINES, ...) call
// backed by one VBO of precomputed {uv, rgba} vertices. Background is a textured quad with a
// checker-pattern fragment path for alpha.
class GpuCanvasRenderer : public CanvasRenderer {
public:
    GpuCanvasRenderer();
    ~GpuCanvasRenderer();

    bool initializeGL();   // Must be called from inside a current OpenGL context.
    void releaseGL();      // Must be called while the context is still current.

    void render(const CanvasView& view, const SceneState& scene);

    // CanvasRenderer
    void onTextureChanged(const QImage* img) override;
    void onMeshesChanged(const std::vector<geom::Mesh>& meshes, const SceneState& scene) override;
    void onSelectionChanged(const std::vector<geom::Mesh>& meshes, const SceneState& scene) override;

private:
    void rebuild_line_vertices(const std::vector<geom::Mesh>& meshes, const SceneState& scene);
    void upload_texture(const QImage& img);
    void release_texture();

    QOpenGLFunctions_3_3_Core* gl_ = nullptr;

    std::unique_ptr<QOpenGLShaderProgram> bg_prog_;
    std::unique_ptr<QOpenGLShaderProgram> wire_prog_;

    QOpenGLBuffer bg_vbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject bg_vao_;

    QOpenGLBuffer wire_vbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject wire_vao_;
    int wire_vertex_count_ = 0;

    std::unique_ptr<QOpenGLTexture> tex_;
    bool has_texture_ = false;

    bool gl_ready_ = false;
};

} // namespace uvc::render
