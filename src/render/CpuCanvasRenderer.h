#pragma once

#include "CanvasRenderer.h"

#include <QImage>
#include <QPainter>

namespace uvc::render {

// Fallback renderer using QPainter. Produces a QImage that can be drawn to a QWidget.
// Slower for large wireframes but works without an OpenGL context (e.g. remote/VM).
class CpuCanvasRenderer : public CanvasRenderer {
public:
    void onTextureChanged(const QImage* img) override;
    void onMeshesChanged(const std::vector<geom::Mesh>& meshes, const SceneState& scene) override;
    void onSelectionChanged(const std::vector<geom::Mesh>& meshes, const SceneState& scene) override;

    QImage render(const CanvasView& view, const SceneState& scene);

private:
    QImage composed_background(const CanvasView& view, const SceneState& scene, int out_w, int out_h) const;

    QImage texture_;
};

} // namespace uvc::render
