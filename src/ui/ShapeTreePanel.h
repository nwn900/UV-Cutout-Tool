#pragma once

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace uvc::geom    { struct Mesh; }
namespace uvc::themes  { struct Theme; }

namespace uvc::ui {

// 1:1 port of the Python shape tree (lines 3301-3562 of UV Cutout Tool.py):
// one row per mesh with a visibility checkbox + clickable name label that
// toggles the entire mesh's triangle selection, plus one nested row per UV
// island that toggles just that island.
class ShapeTreePanel : public QWidget {
    Q_OBJECT
public:
    explicit ShapeTreePanel(QWidget* parent = nullptr);

    // Re-scan the supplied mesh vector and rebuild the row widgets. Call after
    // setMeshes() on the canvas, or whenever island membership changes.
    void rebuild(const std::vector<geom::Mesh>& meshes);

    // Refresh only the row background colors (mesh "all selected" / "partial"
    // / "none" state and per-island selection/hover state) — no widget churn.
    void refreshHighlights(const std::vector<geom::Mesh>& meshes,
                           int hover_mesh_idx = -1,
                           int hover_island_idx = -1);

    void applyTheme(const themes::Theme& t);

    // Update the footer selection counter using the current mesh selection
    // state. Mirrors Python `_refresh_shape_tree` sel_lbl logic (lines
    // 3441-3454): "{N} island(s), {M} triangles" when any tri is selected,
    // else "No triangles selected".
    void refreshSelectionCounter(const std::vector<geom::Mesh>& meshes);

    // Update the live zoom percentage label in the footer. The caller (canvas
    // owner) passes the current zoom factor; the panel renders "{N}%".
    void setZoomPercent(int pct);

signals:
    // Emitted when the user toggles selection by clicking a mesh or island
    // label. The main window handles persisting the state / updating GL.
    void meshSelectionToggleRequested(int mesh_idx);
    void islandSelectionToggleRequested(int mesh_idx, int island_idx);

    // Emitted when the visibility checkbox on a mesh row is clicked — receiver
    // flips the mesh's `visible` flag and calls rebuild().
    void meshVisibilityToggleRequested(int mesh_idx);

    // Footer zoom controls (Python `_zoom_out` / `_zoom_in` / `_zoom_fit`).
    void zoomOutRequested();
    void zoomInRequested();
    void zoomFitRequested();

    // Emitted when the pointer enters or leaves an island row. (-1, -1) means
    // "no island hovered". MainWindow routes this to the canvas so the island
    // lights up in lockstep (Python 3501-3536 `_on_island_row_enter/leave`).
    void islandRowHoverChanged(int mesh_idx, int island_idx);

private:
    void updateSummaryText(int mesh_count, int island_count);
    struct MeshRow {
        QWidget* row = nullptr;
        QLabel*  checkbox = nullptr;   // custom label used as checkbox
        QLabel*  name_lbl = nullptr;
        int      mesh_idx = -1;
    };
    struct IslandRow {
        QWidget* row = nullptr;
        QLabel*  name_lbl = nullptr;
        int      mesh_idx = -1;
        int      island_idx = -1;
    };

    void clear_rows();
    void paint_checkbox(QLabel* cb, bool checked);
    void apply_row_palette(QWidget* w, const QColor& bg);

    QScrollArea* scroll_ = nullptr;
    QWidget*     body_   = nullptr;
    QVBoxLayout* vlay_   = nullptr;
    QLabel*      header_ = nullptr;
    QLabel*      status_ = nullptr;

    // Footer (zoom controls + selection counter) mirrors Python sidebar footer
    // at lines 3274-3299.
    QWidget*     footer_        = nullptr;
    QLabel*      zoom_out_btn_  = nullptr;
    QLabel*      zoom_fit_btn_  = nullptr;
    QLabel*      zoom_in_btn_   = nullptr;
    QLabel*      zoom_pct_lbl_  = nullptr;
    QLabel*      sel_lbl_       = nullptr;

    std::vector<MeshRow>   mesh_rows_;
    std::vector<IslandRow> island_rows_;

    const themes::Theme* theme_ = nullptr;

    bool eventFilter(QObject* obj, QEvent* e) override;
};

} // namespace uvc::ui
