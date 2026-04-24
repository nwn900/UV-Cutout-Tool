#pragma once

#include "../geometry/MeshData.h"
#include "../themes/Theme.h"

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace uvc::render {

// Shared view/transform state, owned by UVCanvasWidget and handed to whichever backend is in use.
struct CanvasView {
    QSize canvas_pixels{1, 1};
    float zoom  = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    bool  alpha_on = true;   // checkerboard background for transparent areas
    // Device pixel ratio of the window the canvas is drawn into. Used to
    // scale the GL viewport up to the physical framebuffer size while all
    // pan/zoom math stays in logical (Qt) pixels. 1.0 on non-HiDPI systems,
    // 1.25 / 1.5 / 2.0 on scaled displays.
    float device_pixel_ratio = 1.0f;
};

struct SceneState {
    const std::vector<geom::Mesh>* meshes = nullptr;

    // Optional active/hover highlights
    int hover_mesh_idx = -1;
    int hover_tri_idx  = -1;
    // When the hover target belongs to a named island, this is the island
    // index (so every triangle in that island gets tinted — Python highlights
    // whole islands on hover, not just the directly-hovered triangle).
    int hover_island_mesh_idx = -1;
    int hover_island_idx      = -1;

    // Colors resolved from the current theme.
    QColor bg_canvas_color {10, 10, 15};
    QColor checker_dark    {26, 26, 26};
    QColor checker_light   {46, 46, 46};
    QColor wire_color      {128, 112,  96};
    QColor hover_color     {200, 200, 128};
    QColor selected_color  {255,  48,  48};

    // Optional marquee drag-rect (in UV space). Empty rect → disabled.
    QRectF drag_rect;

    // Live drag-marquee preview — islands whose bbox intersects `drag_rect`
    // are rendered with `preview_color` (unless they are already selected,
    // which wins). Keys are packed as `(uint64_t(mesh_idx) << 32) | island_idx`.
    // Matches Python `_drag_preview_islands` in lines 4321-4349.
    std::unordered_set<uint64_t> drag_preview_islands;
    QColor preview_color{176, 148, 116};  // default ~ parchment

    // Optional diffuse texture (null → checker/black background only).
    const QImage* diffuse = nullptr;

    // Logical size of the UV 0..1 region in image pixels (width/height of the diffuse,
    // or a default like 1024 when no texture is loaded).
    QSize uv_size{1024, 1024};
};

class CanvasRenderer {
public:
    virtual ~CanvasRenderer() = default;

    // Called when the diffuse texture changes so GPU backends can re-upload.
    virtual void onTextureChanged(const QImage* img) = 0;

    // Called when mesh data changes (new NIF/OBJ loaded, islands rebuilt, selection
    // toggled) so GPU backends can re-upload geometry.
    virtual void onMeshesChanged(const std::vector<geom::Mesh>& meshes,
                                 const SceneState& scene) = 0;

    // Called when only selection/visibility state changed (cheaper than a full upload).
    virtual void onSelectionChanged(const std::vector<geom::Mesh>& meshes,
                                    const SceneState& scene) = 0;
};

} // namespace uvc::render
