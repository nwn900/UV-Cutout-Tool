#include "UVCanvasWidget.h"
#include "../geometry/Geometry.h"
#include "../geometry/IslandBuilder.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFont>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace uvc::ui {

namespace {

bool is_mesh_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "nif";
}

bool is_diffuse_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "png" || ext == "tga" || ext == "dds"
        || ext == "jpg" || ext == "jpeg" || ext == "bmp";
}

QString dropped_local_file(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) return {};
    const auto urls = mime->urls();
    if (urls.isEmpty()) return {};
    QStringList paths;
    for (const auto& url : urls) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    return paths.join("|||");
}

bool has_scene_content(const std::vector<geom::Mesh>& meshes, const QImage& diffuse) {
    return !meshes.empty() || !diffuse.isNull();
}

bool image_has_transparency(const QImage& img) {
    if (img.isNull() || !img.hasAlphaChannel()) return false;
    const QImage rgba = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < rgba.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(rgba.constScanLine(y));
        for (int x = 0; x < rgba.width(); ++x) {
            if (qAlpha(row[x]) < 255) return true;
        }
    }
    return false;
}

} // namespace

UVCanvasWidget::UVCanvasWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    theme_state_.wire_color     = QColor(128, 112, 96);
    theme_state_.hover_color    = QColor(200, 200, 128);
    theme_state_.selected_color = QColor(255,  48,  48);
    updateCursorShape();
}

UVCanvasWidget::~UVCanvasWidget() {
    if (gpu_) {
        makeCurrent();
        gpu_->releaseGL();
        doneCurrent();
    }
}

void UVCanvasWidget::setMeshes(std::vector<geom::Mesh> meshes) {
    meshes_ = std::move(meshes);
    for (auto& m : meshes_) {
        m.islands = geom::compute_islands(m.triangles);
        m.island_visible.clear();
        for (int i = 0; i < int(m.islands.size()); ++i) m.island_visible[i] = true;
    }
    rebuild_spatial_grids();

    if (gpu_ok_ && gpu_) {
        makeCurrent();
        render::SceneState s; build_scene(s);
        gpu_->onMeshesChanged(meshes_, s);
        doneCurrent();
    }
    // Empty-mesh path is the back-to-home clear. Reset transient interaction
    // state and the initial-fit flag so the NEXT load starts a clean session
    // and resizeGL won't chase stale dimensions in the meantime.
    if (meshes_.empty()) {
        hover_mesh_ = hover_tri_ = -1;
        ext_hover_mesh_ = ext_hover_island_ = -1;
        dragging_rect_ = false;
        drag_preview_islands_.clear();
        initial_fit_done_ = false;
        if (diffuse_.isNull()) zoomFit();
    } else {
        zoomFit();
    }
    view_.content_supports_alpha = diffuse_has_alpha_ || (!meshes_.empty() && diffuse_.isNull());
    updateCursorShape();
    emit selectionChanged();
    update();
}

void UVCanvasWidget::setDiffuse(const QImage& img) {
    const bool size_changed = (img.size() != diffuse_.size());
    diffuse_ = img;
    diffuse_has_alpha_ = image_has_transparency(diffuse_);
    view_.content_supports_alpha = diffuse_has_alpha_ || (!meshes_.empty() && diffuse_.isNull());
    if (gpu_ok_ && gpu_) {
        makeCurrent();
        gpu_->onTextureChanged(diffuse_.isNull() ? nullptr : &diffuse_);
        doneCurrent();
    }
    // Whenever the diffuse dimensions change the UV 0..1 region maps to a new
    // pixel box, so the previous fit is stale. Re-fit so a 2048x2048 diffuse
    // loaded after a 1024-default fit (or any two different textures across a
    // session) ends up centered and sized to the canvas. This also catches
    // the texture-first, mesh-second load order.
    if (size_changed && !diffuse_.isNull()) zoomFit();
    else if (diffuse_.isNull() && meshes_.empty()) zoomFit();
    updateCursorShape();
    update();
}

void UVCanvasWidget::setThemeColors(const QColor& wire, const QColor& hover, const QColor& selected) {
    theme_state_.wire_color     = wire;
    theme_state_.hover_color    = hover;
    theme_state_.selected_color = selected;
    refreshSelection();
}

void UVCanvasWidget::setEmptyStateColors(const QColor& text, const QColor& panel) {
    empty_text_color_ = text;
    empty_panel_color_ = panel;
    update();
}

void UVCanvasWidget::setBackgroundColors(const QColor& canvas_bg, const QColor& checker_dark, const QColor& checker_light) {
    canvas_bg_color_ = canvas_bg;
    checker_dark_color_ = checker_dark;
    checker_light_color_ = checker_light;
    update();
}

void UVCanvasWidget::setAlphaEnabled(bool on) {
    view_.alpha_on = on;
    update();
}

void UVCanvasWidget::updateCursorShape() {
    if (panning_) {
        setCursor(Qt::ClosedHandCursor);
    } else if (space_held_) {
        setCursor(Qt::OpenHandCursor);
    } else if (hover_mesh_ >= 0 && hover_tri_ >= 0) {
        setCursor(Qt::PointingHandCursor);
    } else if (!meshes_.empty() || !diffuse_.isNull()) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void UVCanvasWidget::zoomFit() {
    if (width() <= 0 || height() <= 0) return;
    // Fit the actual UV-space width/height into the canvas and clamp to 100%
    // so small textures do not magnify above 1:1.
    const float W = diffuse_.isNull() ? 1024.0f : float(diffuse_.width());
    const float H = diffuse_.isNull() ? 1024.0f : float(diffuse_.height());
    const float margin = 20.0f;
    const float cw = std::max(1.0f, float(width())  - margin * 2.0f);
    const float ch = std::max(1.0f, float(height()) - margin * 2.0f);
    view_.zoom  = std::min({cw / W, ch / H, 1.0f});
    view_.zoom  = std::max(view_.zoom, 0.05f);
    view_.pan_x = (float(width())  - W * view_.zoom) * 0.5f;
    view_.pan_y = (float(height()) - H * view_.zoom) * 0.5f;
    initial_fit_done_ = true;
    last_w_ = width();
    last_h_ = height();
    emit zoomChanged(zoomPercent());
    update();
}

int UVCanvasWidget::zoomPercent() const {
    return int(std::round(view_.zoom * 100.f));
}

void UVCanvasWidget::zoomInStep() {
    if (!has_scene_content(meshes_, diffuse_)) return;
    view_.zoom = std::clamp(view_.zoom * 1.25f, 0.02f, 100.0f);
    const float W = diffuse_.isNull() ? 1024.f : float(diffuse_.width());
    const float H = diffuse_.isNull() ? 1024.f : float(diffuse_.height());
    view_.pan_x = (float(width())  - W * view_.zoom) * 0.5f;
    view_.pan_y = (float(height()) - H * view_.zoom) * 0.5f;
    emit zoomChanged(zoomPercent());
    update();
}

void UVCanvasWidget::zoomOutStep() {
    if (!has_scene_content(meshes_, diffuse_)) return;
    view_.zoom = std::clamp(view_.zoom / 1.25f, 0.02f, 100.0f);
    const float W = diffuse_.isNull() ? 1024.f : float(diffuse_.width());
    const float H = diffuse_.isNull() ? 1024.f : float(diffuse_.height());
    view_.pan_x = (float(width())  - W * view_.zoom) * 0.5f;
    view_.pan_y = (float(height()) - H * view_.zoom) * 0.5f;
    emit zoomChanged(zoomPercent());
    update();
}

void UVCanvasWidget::refreshSelection() {
    if (gpu_ok_ && gpu_) {
        makeCurrent();
        render::SceneState s; build_scene(s);
        gpu_->onSelectionChanged(meshes_, s);
        doneCurrent();
    }
    update();
}

void UVCanvasWidget::rebuildSpatialGrids() {
    rebuild_spatial_grids();
}

void UVCanvasWidget::initializeGL() {
    gpu_ = std::make_unique<render::GpuCanvasRenderer>();
    gpu_ok_ = gpu_->initializeGL();
    if (gpu_ok_ && !diffuse_.isNull())
        gpu_->onTextureChanged(&diffuse_);
    if (gpu_ok_ && !meshes_.empty()) {
        render::SceneState s; build_scene(s);
        gpu_->onMeshesChanged(meshes_, s);
    }
}

void UVCanvasWidget::showEvent(QShowEvent* e) {
    QOpenGLWidget::showEvent(e);
    // Fire a deferred re-fit so we measure the canvas AFTER the QStackedWidget
    // switch, the toolbar show, and the dock show have all propagated through
    // the central-widget layout. 50 ms matches the resize debounce and is
    // imperceptible on workspace entry.
    if (!meshes_.empty() || !diffuse_.isNull()) {
        // Re-check inside the lambda: the user could click Back-to-Home
        // during the 50 ms window (which calls setMeshes({}) and
        // setDiffuse(QImage())), and then a fit on an empty canvas would
        // just waste work while the widget is already hidden again.
        QTimer::singleShot(50, this, [this] {
            if (!meshes_.empty() || !diffuse_.isNull()) zoomFit();
        });
    }
}

void UVCanvasWidget::resizeGL(int w, int h) {
    view_.canvas_pixels = QSize(w, h);
    // Only re-fit once the initial fit has run, once meshes are loaded, and
    // only when the delta exceeds a 5px debounce threshold.
    // before we sample the final canvas size.
    if (!initial_fit_done_ || meshes_.empty()) return;
    if (w <= 1 || h <= 1) return;
    if (std::abs(w - last_w_) < 5 && std::abs(h - last_h_) < 5) return;
    last_w_ = w;
    last_h_ = h;
    QTimer::singleShot(50, this, [this] {
        if (!meshes_.empty() || !diffuse_.isNull()) zoomFit();
    });
}

void UVCanvasWidget::dragEnterEvent(QDragEnterEvent* e) {
    const QString paths = dropped_local_file(e->mimeData());
    if (paths.isEmpty()) {
        e->ignore();
        return;
    }
    const auto parts = paths.split("|||");
    for (const QString& p : parts) {
        if (is_mesh_file(p) || is_diffuse_file(p)) {
            e->acceptProposedAction();
            return;
        }
    }
    e->ignore();
}

void UVCanvasWidget::dragMoveEvent(QDragMoveEvent* e) {
    const QString paths = dropped_local_file(e->mimeData());
    if (paths.isEmpty()) {
        e->ignore();
        return;
    }
    const auto parts = paths.split("|||");
    for (const QString& p : parts) {
        if (is_mesh_file(p) || is_diffuse_file(p)) {
            e->acceptProposedAction();
            return;
        }
    }
    e->ignore();
}

void UVCanvasWidget::dropEvent(QDropEvent* e) {
    const QString paths = dropped_local_file(e->mimeData());
    if (paths.isEmpty()) {
        e->ignore();
        return;
    }
    const auto parts = paths.split("|||");
    bool mesh_emitted = false, diffuse_emitted = false;
    for (const QString& p : parts) {
        if (is_mesh_file(p)) {
            emit meshFileDropped(p);
            mesh_emitted = true;
        }
        else if (is_diffuse_file(p) && !diffuse_emitted) {
            emit diffuseFileDropped(p);
            diffuse_emitted = true;
        }
    }
    if (mesh_emitted || diffuse_emitted) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void UVCanvasWidget::paintGL() {
    render::SceneState s; build_scene(s);
    view_.canvas_pixels      = QSize(width(), height());
    view_.device_pixel_ratio = float(devicePixelRatioF());
    if (gpu_ok_ && gpu_) {
        gpu_->render(view_, s);
    } else {
        render::CpuCanvasRenderer cpu;
        cpu.onTextureChanged(diffuse_.isNull() ? nullptr : &diffuse_);
        const QImage img = cpu.render(view_, s);
        QPainter fallback(this);
        fallback.drawImage(rect(), img);
    }

    if (!s.drag_rect.isEmpty()) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const float uvw = s.uv_size.width()  * view_.zoom;
        const float uvh = s.uv_size.height() * view_.zoom;
        QRectF r(view_.pan_x + s.drag_rect.x() * uvw,
                 view_.pan_y + s.drag_rect.y() * uvh,
                 s.drag_rect.width()  * uvw,
                 s.drag_rect.height() * uvh);
        p.setPen(QPen(theme_state_.selected_color, 1.0, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }

    if (meshes_.empty() && diffuse_.isNull()) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRect bubble((width() - 392) / 2, (height() - 104) / 2, 392, 104);
        p.setPen(Qt::NoPen);
        p.setBrush(empty_panel_color_);
        p.drawRoundedRect(bubble, 14, 14);

        p.setPen(empty_text_color_);
        QFont title("Georgia", 14, QFont::Bold);
        p.setFont(title);
        p.drawText(QRect(bubble.left() + 18, bubble.top() + 18, bubble.width() - 36, 30),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   "Load files to begin");

        p.setPen(empty_text_color_.darker(115));
        QFont body("Segoe UI", 9);
        p.setFont(body);
        p.drawText(QRect(bubble.left() + 18, bubble.top() + 54, bubble.width() - 36, 32),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   "Drop a mesh or texture here, or use the buttons to load them.");
    }
}

void UVCanvasWidget::build_scene(render::SceneState& scene) const {
    scene.meshes = &meshes_;
    scene.hover_mesh_idx = hover_mesh_;
    scene.hover_tri_idx  = hover_tri_;

    // External (sidebar) hover wins over pointer hover so the two UIs stay in
    // lockstep — when the user is hovering a row in the shape tree, we paint
    // that island even though the pointer is elsewhere.
    if (ext_hover_mesh_ >= 0 && ext_hover_island_ >= 0) {
        scene.hover_island_mesh_idx = ext_hover_mesh_;
        scene.hover_island_idx      = ext_hover_island_;
    } else if (hover_mesh_ >= 0 && hover_tri_ >= 0 &&
               hover_mesh_ < int(meshes_.size()) &&
               hover_tri_  < int(meshes_[hover_mesh_].triangles.size()) &&
               meshes_[hover_mesh_].triangles[hover_tri_].island_id.has_value()) {
        scene.hover_island_mesh_idx = hover_mesh_;
        scene.hover_island_idx      = *meshes_[hover_mesh_].triangles[hover_tri_].island_id;
    }

    scene.bg_canvas_color = canvas_bg_color_;
    scene.checker_dark = checker_dark_color_;
    scene.checker_light = checker_light_color_;
    scene.wire_color     = theme_state_.wire_color;
    scene.hover_color    = theme_state_.hover_color;
    scene.selected_color = theme_state_.selected_color;
    scene.diffuse        = diffuse_.isNull() ? nullptr : &diffuse_;
    scene.uv_size        = diffuse_.isNull() ? QSize(1024, 1024) : diffuse_.size();
    if (dragging_rect_) {
        const float x1 = std::min(drag_start_uv_.x(), drag_current_uv_.x());
        const float y1 = std::min(drag_start_uv_.y(), drag_current_uv_.y());
        const float x2 = std::max(drag_start_uv_.x(), drag_current_uv_.x());
        const float y2 = std::max(drag_start_uv_.y(), drag_current_uv_.y());
        scene.drag_rect = QRectF(x1, y1, x2 - x1, y2 - y1);
    }
    scene.drag_preview_islands = drag_preview_islands_;
    scene.preview_color        = preview_color_;
}

void UVCanvasWidget::setPreviewColor(const QColor& c) {
    preview_color_ = c;
    if (!drag_preview_islands_.empty()) refreshSelection();
}

void UVCanvasWidget::rebuild_spatial_grids() {
    spatial_grids_.clear();
    for (int mi = 0; mi < int(meshes_.size()); ++mi) {
        const auto& m = meshes_[mi];
        if (!m.visible) continue;
        geom::SpatialGrid g(0.02f);
        for (int ti = 0; ti < int(m.triangles.size()); ++ti)
            g.insert(ti, m.triangles[ti].bbox);
        spatial_grids_.emplace(mi, std::move(g));
    }
}

QPointF UVCanvasWidget::screen_to_uv(QPointF p) const {
    const float uvw = (diffuse_.isNull() ? 1024.f : diffuse_.width())  * view_.zoom;
    const float uvh = (diffuse_.isNull() ? 1024.f : diffuse_.height()) * view_.zoom;
    return QPointF((p.x() - view_.pan_x) / uvw, (p.y() - view_.pan_y) / uvh);
}

int UVCanvasWidget::hit_test(QPointF uv, int* out_mesh_idx) const {
    for (int mi = int(meshes_.size()) - 1; mi >= 0; --mi) {
        const auto& m = meshes_[mi];
        if (!m.visible) continue;
        auto it = spatial_grids_.find(mi);
        if (it == spatial_grids_.end()) continue;
        const auto candidates = it->second.query(float(uv.x()), float(uv.y()));
        for (int ti : candidates) {
            if (geom::point_in_triangle_barycentric(float(uv.x()), float(uv.y()), m.triangles[ti].uv)) {
                if (out_mesh_idx) *out_mesh_idx = mi;
                return ti;
            }
        }
    }
    return -1;
}

int UVCanvasWidget::count_tris_at(QPointF uv) const {
    int count = 0;
    for (int mi = 0; mi < int(meshes_.size()); ++mi) {
        const auto& m = meshes_[mi];
        if (!m.visible) continue;
        auto it = spatial_grids_.find(mi);
        if (it == spatial_grids_.end()) continue;
        const auto candidates = it->second.query(float(uv.x()), float(uv.y()));
        for (int ti : candidates) {
            if (geom::point_in_triangle_barycentric(float(uv.x()), float(uv.y()), m.triangles[ti].uv))
                ++count;
        }
    }
    return count;
}

int UVCanvasWidget::global_island_id(int mesh_idx, int island_idx) const {
    if (mesh_idx < 0 || mesh_idx >= int(meshes_.size())) return 0;
    int g = 0;
    for (int i = 0; i < mesh_idx; ++i) g += int(meshes_[i].islands.size());
    return g + island_idx + 1;
}

void UVCanvasWidget::mousePressEvent(QMouseEvent* e) {
    setFocus();
    const bool has_content = has_scene_content(meshes_, diffuse_);
    // Pan on middle, right, or space+left.
    if (has_content && (e->button() == Qt::MiddleButton
        || e->button() == Qt::RightButton
        || (e->button() == Qt::LeftButton && space_held_))) {
        panning_ = true;
        pan_anchor_ = e->position();
        updateCursorShape();
        return;
    }
    if (e->button() == Qt::LeftButton) {
        QPointF uv = screen_to_uv(e->position());
        int mi = -1;
        int ti = hit_test(uv, &mi);
        if (ti >= 0) {
            // Clicking flips the whole clicked island between all-selected
            // and not-selected, without clearing any other selection.
            // This is what makes multi-click accumulation work: every
            // click toggles one more island on or off without touching
            // anything the user already selected.
            emit selectionAboutToChange();
            auto& mesh = meshes_[mi];
            auto& tri  = mesh.triangles[ti];
            if (tri.island_id.has_value()) {
                const int iid = *tri.island_id;
                bool any_sel = false;
                for (const auto& t : mesh.triangles) {
                    if (t.island_id.has_value() && *t.island_id == iid && t.selected) {
                        any_sel = true; break;
                    }
                }
                const bool new_state = !any_sel;
                for (auto& t : mesh.triangles) {
                    if (t.island_id.has_value() && *t.island_id == iid)
                        t.selected = new_state;
                }
            } else {
                tri.selected = !tri.selected;
            }
            refreshSelection();
            emit selectionChanged();
        } else {
            // Start marquee drag.
            drag_start_uv_  = uv;
            drag_current_uv_ = uv;
            dragging_rect_ = true;
            updateCursorShape();
        }
    }
}

void UVCanvasWidget::mouseMoveEvent(QMouseEvent* e) {
    if (panning_) {
        QPointF d = e->position() - pan_anchor_;
        view_.pan_x += float(d.x());
        view_.pan_y += float(d.y());
        pan_anchor_ = e->position();
        updateCursorShape();
        update();
        return;
    }

    QPointF uv = screen_to_uv(e->position());
    if (dragging_rect_) {
        drag_current_uv_ = uv;
        // Recompute which islands intersect the current marquee so the GPU
        // wireframe rebuild paints them in the preview color.
        const float ru1 = float(std::min(drag_start_uv_.x(), drag_current_uv_.x()));
        const float rv1 = float(std::min(drag_start_uv_.y(), drag_current_uv_.y()));
        const float ru2 = float(std::max(drag_start_uv_.x(), drag_current_uv_.x()));
        const float rv2 = float(std::max(drag_start_uv_.y(), drag_current_uv_.y()));
        std::unordered_set<uint64_t> next;
        for (int mi = 0; mi < int(meshes_.size()); ++mi) {
            const auto& m = meshes_[mi];
            if (!m.visible) continue;
            for (const auto& t : m.triangles) {
                if (!t.island_id.has_value()) continue;
                if (t.bbox.max_u < ru1 || t.bbox.min_u > ru2) continue;
                if (t.bbox.max_v < rv1 || t.bbox.min_v > rv2) continue;
                uint64_t key = (uint64_t(mi) << 32) | uint32_t(*t.island_id);
                next.insert(key);
            }
        }
        if (next != drag_preview_islands_) {
            drag_preview_islands_ = std::move(next);
            refreshSelection();
        } else {
            update();
        }
        return;
    }

    int mi = -1;
    int ti = hit_test(uv, &mi);
    if (mi != hover_mesh_ || ti != hover_tri_) {
        hover_mesh_ = mi;
        hover_tri_  = ti;
        updateCursorShape();
        refreshSelection();
        QString info;
        int hover_island = -1;
        if (mi >= 0) {
            const auto& tri = meshes_[mi].triangles[ti];
            if (tri.island_id.has_value()) {
                hover_island = *tri.island_id;
                int gid  = global_island_id(mi, *tri.island_id);
                int tris = int(meshes_[mi].islands[*tri.island_id].size());
                if (gid > 0)
                    info = QString("UV Island %1 (%2 tris)").arg(gid).arg(tris);
            } else {
                int overlapping = count_tris_at(uv);
                if (overlapping <= 1)
                    info = QString("Mesh %1, Tri %2").arg(mi).arg(ti);
                else
                    info = QString("%1 overlapping tris").arg(overlapping);
            }
        }
        emit hoverInfoChanged(info);
        emit hoverIslandChanged(mi, hover_island);
    }
}

void UVCanvasWidget::setExternalIslandHover(int mesh_idx, int island_idx) {
    if (ext_hover_mesh_ == mesh_idx && ext_hover_island_ == island_idx) return;
    ext_hover_mesh_   = mesh_idx;
    ext_hover_island_ = island_idx;
    updateCursorShape();
    refreshSelection();
}

void UVCanvasWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::MiddleButton
        || e->button() == Qt::RightButton
        || (panning_ && e->button() == Qt::LeftButton)) {
        panning_ = false;
        updateCursorShape();
        return;
    }
    if (e->button() == Qt::LeftButton && dragging_rect_) {
        dragging_rect_ = false;
        drag_preview_islands_.clear();
        // Collect every island whose bbox intersects the marquee, then set every triangle
        // island whose bbox intersects the marquee, then set every triangle
        // in those islands to selected. This is additive; existing selections
        // are not cleared.
        const float x1 = std::min(drag_start_uv_.x(), drag_current_uv_.x());
        const float y1 = std::min(drag_start_uv_.y(), drag_current_uv_.y());
        const float x2 = std::max(drag_start_uv_.x(), drag_current_uv_.x());
        const float y2 = std::max(drag_start_uv_.y(), drag_current_uv_.y());
        emit selectionAboutToChange();
        for (auto& m : meshes_) {
            if (!m.visible) continue;
            std::unordered_set<int> islands_in_box;
            for (const auto& t : m.triangles) {
                if (t.bbox.max_u < x1 || t.bbox.min_u > x2) continue;
                if (t.bbox.max_v < y1 || t.bbox.min_v > y2) continue;
                if (t.island_id.has_value()) islands_in_box.insert(*t.island_id);
            }
            for (auto& t : m.triangles) {
                if (t.island_id.has_value() &&
                    islands_in_box.count(*t.island_id))
                    t.selected = true;
            }
        }
        refreshSelection();
        emit selectionChanged();
        updateCursorShape();
    }
}

void UVCanvasWidget::wheelEvent(QWheelEvent* e) {
    if (!has_scene_content(meshes_, diffuse_)) {
        e->accept();
        return;
    }
    if (scroll_throttle_) { e->accept(); return; }
    scroll_throttle_ = true;
    QTimer::singleShot(10, this, [this]{ scroll_throttle_ = false; });

    const bool up = e->angleDelta().y() > 0;
    const float factor = up ? 1.15f : 0.87f;
    QPointF uv_before = screen_to_uv(e->position());
    view_.zoom = std::clamp(view_.zoom * factor, 0.02f, 100.0f);

    const float W = diffuse_.isNull() ? 1024.f : float(diffuse_.width());
    const float H = diffuse_.isNull() ? 1024.f : float(diffuse_.height());
    const float cw = float(width());
    const float ch = float(height());

    // Once the scaled image fits entirely in the canvas on both axes, snap
    // back to a centered layout instead of letting the cursor anchor drift it off-center.
    if (W * view_.zoom <= cw && H * view_.zoom <= ch) {
        view_.pan_x = (cw - W * view_.zoom) * 0.5f;
        view_.pan_y = (ch - H * view_.zoom) * 0.5f;
    } else {
        view_.pan_x = float(e->position().x()) - float(uv_before.x()) * W * view_.zoom;
        view_.pan_y = float(e->position().y()) - float(uv_before.y()) * H * view_.zoom;
    }
    emit zoomChanged(zoomPercent());
    update();
}

void UVCanvasWidget::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space) {
        space_held_ = true;
        updateCursorShape();
    }
    else QOpenGLWidget::keyPressEvent(e);
}
void UVCanvasWidget::keyReleaseEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space) {
        space_held_ = false;
        updateCursorShape();
    }
    else QOpenGLWidget::keyReleaseEvent(e);
}

void UVCanvasWidget::focusOutEvent(QFocusEvent* e) {
    QOpenGLWidget::focusOutEvent(e);
    const bool had_state =
        hover_mesh_ != -1 || hover_tri_ != -1 ||
        dragging_rect_ || panning_ || space_held_ ||
        !drag_preview_islands_.empty() ||
        ext_hover_mesh_ != -1 || ext_hover_island_ != -1;
    hover_mesh_ = -1;
    hover_tri_  = -1;
    ext_hover_mesh_ = -1;
    ext_hover_island_ = -1;
    dragging_rect_ = false;
    panning_ = false;
    space_held_ = false;
    drag_preview_islands_.clear();
    updateCursorShape();
    if (had_state) {
        emit hoverInfoChanged(QString());
        emit hoverIslandChanged(-1, -1);
        emit interactionStateCleared();
        refreshSelection();
    }
}

void UVCanvasWidget::leaveEvent(QEvent* e) {
    QOpenGLWidget::leaveEvent(e);
    const bool had_state =
        hover_mesh_ != -1 || hover_tri_ != -1 ||
        !drag_preview_islands_.empty();
    hover_mesh_ = -1;
    hover_tri_  = -1;
    if (!dragging_rect_) drag_preview_islands_.clear();
    updateCursorShape();
    if (had_state) {
        emit hoverInfoChanged(QString());
        emit hoverIslandChanged(-1, -1);
        emit interactionStateCleared();
        refreshSelection();
    }
}

} // namespace uvc::ui
