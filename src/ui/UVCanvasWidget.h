#pragma once

#include "../geometry/MeshData.h"
#include "../geometry/SpatialGrid.h"
#include "../render/CanvasRenderer.h"
#include "../render/CpuCanvasRenderer.h"
#include "../render/GpuCanvasRenderer.h"

#include <QImage>
#include <QOpenGLWidget>
#include <QPointF>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

namespace uvc::ui {

class UVCanvasWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit UVCanvasWidget(QWidget* parent = nullptr);
    ~UVCanvasWidget() override;

    void setMeshes(std::vector<geom::Mesh> meshes);
    const std::vector<geom::Mesh>& meshes() const { return meshes_; }
    std::vector<geom::Mesh>& meshes_mut() { return meshes_; }

    void setDiffuse(const QImage& img);
    const QImage& diffuse() const { return diffuse_; }

    void setThemeColors(const QColor& wire, const QColor& hover, const QColor& selected);
    void setEmptyStateColors(const QColor& text, const QColor& panel);
    void setBackgroundColors(const QColor& canvas_bg, const QColor& checker_dark, const QColor& checker_light);
    void setAlphaEnabled(bool on);
    bool alphaEnabled() const { return view_.alpha_on; }

    // Fit the UV 0..1 square into the current canvas size.
    void zoomFit();
    // Zoom in/out about the center (sidebar footer buttons — Python `_zoom_in`
    // / `_zoom_out` / `_center_view`).
    void zoomInStep();
    void zoomOutStep();
    // Current zoom expressed as a percentage (for the footer label).
    int  zoomPercent() const;

    // Force a rebuild of GPU wireframe geometry after external selection changes.
    void refreshSelection();

    // Rebuild the spatial hash grids — needed after visibility toggles so hidden
    // meshes are excluded from hover/click hit-testing (matches Python
    // `_build_spatial_grids` called from visibility toggle paths).
    void rebuildSpatialGrids();

    // External highlight override: when the user hovers a row in the sidebar,
    // the canvas should tint that island as if it were the hover target.
    // (-1, -1) clears the override. Takes precedence over mouse-driven hover
    // so the two surfaces stay in lockstep.
    void setExternalIslandHover(int mesh_idx, int island_idx);

signals:
    void hoverInfoChanged(const QString& text);
    void selectionChanged();
    // Fired whenever the internal zoom factor changes (wheel, fit, zoomIn/Out)
    // so the sidebar footer can mirror the percentage.
    void zoomChanged(int percent);
    // Fired when hover switches to a different island (or off one). Drives the
    // sidebar row highlight — Python does the same in `_do_hover_xy` by
    // reaching into `_last_hovered_island_lbl`. (-1, -1) → no island hovered.
    void hoverIslandChanged(int mesh_idx, int island_idx);
    // Fired the instant BEFORE a user interaction modifies the selection state.
    // MainWindow hooks this to push an undo snapshot (matches Python
    // `_save_selection_state()` calls scattered through the interaction paths).
    void selectionAboutToChange();
    // Emitted when the canvas clears its transient interaction state (focus
    // loss or cursor leave). MainWindow routes this back to the shape-tree so
    // island rows also unhighlight, and to the status bar so the intro hint
    // replaces any stale hover text. Mirrors the trailing cleanup in Python
    // `_on_focus_out` (lines 4423-4451) and `_on_leave` (lines 4401-4421).
    void interactionStateCleared();
    void meshFileDropped(const QString& path);
    void diffuseFileDropped(const QString& path);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    // Clear transient interaction state (hover, drag-rect, drag-preview, pan,
    // space-held). Mirrors Python `_on_focus_out` at lines 4423-4451 — keeps
    // the canvas from "remembering" a half-finished drag when the user tabs
    // away mid-interaction.
    void focusOutEvent(QFocusEvent* e) override;
    void leaveEvent(QEvent* e) override;
    // Re-fit on show: when the stacked widget switches from welcome to canvas,
    // the canvas finally gets its "live" size (toolbar + dock ripple through
    // the central-widget layout). The initial fit inside setMeshes may have
    // been computed against a pre-layout width — defer a refit via QTimer so
    // we sample the canvas AFTER Qt settles the new layout.
    void showEvent(QShowEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void updateCursorShape();
    void rebuild_spatial_grids();
    QPointF screen_to_uv(QPointF p) const;
    int hit_test(QPointF uv, int* out_mesh_idx) const;
    // Count all triangles whose barycentric contains `uv`, across every visible
    // mesh. Used by the hover text formatter to emit "N overlapping tris" when
    // the topmost hit triangle has no island assignment.
    int count_tris_at(QPointF uv) const;
    // Map (mesh, island) to the 1-based running counter across all meshes,
    // mirroring Python's `_island_to_global` mapping built in `_rebuild_uv_data`.
    int global_island_id(int mesh_idx, int island_idx) const;
    void build_scene(render::SceneState& scene) const;

    std::vector<geom::Mesh> meshes_;
    QImage diffuse_;

    std::unordered_map<int, geom::SpatialGrid> spatial_grids_;

    render::CanvasView  view_{};
    render::SceneState  theme_state_{};   // holds colors only

    std::unique_ptr<render::GpuCanvasRenderer> gpu_;
    bool gpu_ok_ = false;

    // Interaction state.
    bool  space_held_   = false;
    bool  panning_      = false;
    QPointF pan_anchor_;
    QPointF drag_start_uv_;
    bool  dragging_rect_ = false;
    QPointF drag_current_uv_;
    int   hover_mesh_ = -1;
    int   hover_tri_  = -1;
    int   ext_hover_mesh_ = -1;
    int   ext_hover_island_ = -1;

    // Resize re-fit state (matches Python `_last_canvas_w/h` debouncing).
    bool  initial_fit_done_ = false;
    int   last_w_ = 0, last_h_ = 0;

    // Set of islands currently intersected by the drag marquee — rendered in
    // a distinct preview color until the drag ends. Keys packed as
    // `(uint64_t(mesh) << 32) | island`.
    std::unordered_set<uint64_t> drag_preview_islands_;
    QColor preview_color_{176, 148, 116};
    QColor canvas_bg_color_{10, 10, 15};
    QColor checker_dark_color_{26, 26, 26};
    QColor checker_light_color_{46, 46, 46};
    QColor empty_text_color_{232, 232, 240};
    QColor empty_panel_color_{28, 28, 36, 220};

    // 10 ms scroll-wheel throttle. Matches Python `_on_scroll` (lines
    // 4612-4616) which drops wheel events delivered inside a 10 ms window
    // to keep zoom smooth on fast trackpads / high-resolution wheels.
    bool scroll_throttle_ = false;

public:
    void setPreviewColor(const QColor& c);
};

} // namespace uvc::ui
