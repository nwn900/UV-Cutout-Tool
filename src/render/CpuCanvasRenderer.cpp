#include "CpuCanvasRenderer.h"

namespace uvc::render {

void CpuCanvasRenderer::onTextureChanged(const QImage* img) {
    texture_ = (img && !img->isNull()) ? img->convertToFormat(QImage::Format_RGBA8888) : QImage{};
}
void CpuCanvasRenderer::onMeshesChanged(const std::vector<geom::Mesh>&, const SceneState&) {}
void CpuCanvasRenderer::onSelectionChanged(const std::vector<geom::Mesh>&, const SceneState&) {}

QImage CpuCanvasRenderer::composed_background(const CanvasView& view, const SceneState& scene, int out_w, int out_h) const {
    QImage img(out_w, out_h, QImage::Format_RGBA8888);
    img.fill(scene.bg_canvas_color);
    QPainter p(&img);
    const bool show_checker = view.alpha_on && view.content_supports_alpha;
    if (show_checker) {
        // Simple checkerboard in screen space.
        const int cs = 16;
        for (int y = 0; y < out_h; y += cs) {
            for (int x = 0; x < out_w; x += cs) {
                bool odd = ((x / cs) + (y / cs)) & 1;
                p.fillRect(x, y, cs, cs, odd ? scene.checker_light : scene.checker_dark);
            }
        }
    }

    const float uvw = scene.uv_size.width()  * view.zoom;
    const float uvh = scene.uv_size.height() * view.zoom;
    if (!texture_.isNull()) {
        QRectF dst(view.pan_x, view.pan_y, uvw, uvh);
        p.drawImage(dst, texture_, texture_.rect());
    }
    return img;
}

QImage CpuCanvasRenderer::render(const CanvasView& view, const SceneState& scene) {
    const int w = view.canvas_pixels.width();
    const int h = view.canvas_pixels.height();
    if (w <= 0 || h <= 0) return {};

    QImage img = composed_background(view, scene, w, h);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    const float uvw = scene.uv_size.width()  * view.zoom;
    const float uvh = scene.uv_size.height() * view.zoom;

    auto to_screen = [&](const geom::UV& uv) {
        return QPointF(view.pan_x + uv.u * uvw, view.pan_y + uv.v * uvh);
    };

    if (!scene.meshes) return img;

    const auto& meshes = *scene.meshes;

    QPen wire_pen  (scene.wire_color,     1.0);
    QPen hover_pen (scene.hover_color,    1.5);
    QPen sel_pen   (scene.selected_color, 1.5);

    for (size_t mi = 0; mi < meshes.size(); ++mi) {
        const auto& m = meshes[mi];
        if (!m.visible) continue;
        for (size_t ti = 0; ti < m.triangles.size(); ++ti) {
            const auto& t = m.triangles[ti];
            bool hover = (int(mi) == scene.hover_mesh_idx && int(ti) == scene.hover_tri_idx);
            if (!hover && scene.hover_island_idx >= 0 &&
                int(mi) == scene.hover_island_mesh_idx &&
                t.island_id.has_value() &&
                *t.island_id == scene.hover_island_idx) {
                hover = true;
            }

            bool preview = false;
            if (!scene.drag_preview_islands.empty() && t.island_id.has_value()) {
                const uint64_t key = (uint64_t(mi) << 32) | uint32_t(*t.island_id);
                preview = scene.drag_preview_islands.count(key) != 0;
            }

            QPen pen = wire_pen;
            if (t.selected)          pen = sel_pen;
            else if (preview)        pen = QPen(scene.preview_color, 1.5);
            else if (hover)          pen = hover_pen;
            p.setPen(pen);
            const QPointF a = to_screen(t.uv[0]);
            const QPointF b = to_screen(t.uv[1]);
            const QPointF c = to_screen(t.uv[2]);
            p.drawLine(a, b);
            p.drawLine(b, c);
            p.drawLine(c, a);
        }
    }

    if (!scene.drag_rect.isEmpty()) {
        QRectF r(view.pan_x + scene.drag_rect.x() * uvw,
                 view.pan_y + scene.drag_rect.y() * uvh,
                 scene.drag_rect.width()  * uvw,
                 scene.drag_rect.height() * uvh);
        p.setPen(QPen(scene.selected_color, 1.0, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }
    return img;
}

} // namespace uvc::render
