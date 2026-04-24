# UV Cutout Tool — C++ / Qt6 / OpenGL Port

A GPU-accelerated port of `UV Cutout Tool.py`. The UI is Qt6 and the canvas is
a `QOpenGLWidget` that batches the entire UV wireframe (often 3000+ edges) into
a **single `glDrawArrays` call**, replacing the Tk `canvas.create_line` loop that
dominated Python render time.

A CPU fallback (`QPainter`) is provided for environments without OpenGL 3.3.

## What has been ported

Complete:
- **BC1 / BC3 / BC4 / BC5 / BC7** block decoders (custom C++ implementation, with BC7
  behavior being tightened toward DirectXTex-equivalent output)
- **DDS loader** (DXT1-5, ATI1/2, DX10 BC1/BC3/BC4/BC5/BC7)
- **TGA writer** (32-bit RGBA uncompressed, top-left origin — byte-for-byte same header as Python)
- **OBJ parser** (vertices, uv coords, faces, groups/objects; triangulates n-gons like the Python)
- **NIF parser** (dynamically loads `NiflyDLL.dll`; same C function surface as the Python `ctypes` bindings)
- **SpatialGrid**, **point-in-triangle barycentric**, **island builder** (shared-UV BFS)
- **All 34 themes** — extracted from the Python source into `resources/themes.json` as the baseline palette set, with targeted post-port refinements where needed to better match the intended theme identity in the Qt UI
- **OpenGL canvas** — textured quad background + checker overlay + single-draw-call wireframe
- **Qt6 main window** — welcome screen, toolbar, canvas, status bar, theme menu
- **Interactions** — click-to-select (island), Shift/Ctrl modifiers, marquee drag, middle/space pan, wheel zoom (cursor-anchored), hover highlight
- **Export TGA / PNG** — rasterizes selected UV triangles into a mask and multiplies the diffuse through it
- **Shape tree side-panel** — fixed right-side workspace panel with per-mesh visibility checkbox, click-to-toggle-mesh-selection, and nested per-island rows that toggle individual UV islands (matches Python `_refresh_shape_tree`)
- **Undo / redo** — Ctrl+Z and Ctrl+Shift+Z, 50-state rolling snapshot of the per-triangle selection bits (matches Python `_save_selection_state` / `_undo` / `_redo`)
- **Theme picker** — categorized popup with expandable `THEME_ORDER` sections (Basic, Environment, Factions, Divines, Daedra), anchored beneath the welcome "Theme: …" pill — 1:1 with Python `_show_theme_window`
- **Welcome ornamentation** — footer rule, "Supports NIF · PNG · TGA · DDS | F11 Fullscreen" strip, and nexusmods credits link
- **F11 fullscreen** — toggle on F11, exit on Escape
- **Progressive load status** — NIF parse callbacks and texture decoding feed messages into the workspace status bar during load operations
- **Right-click panning** — matches Python `<ButtonPress-3>` binding (alongside middle-click and Space+left-drag)
- **Hover text format** — 3-case formatter ("UV Island N (T tris)" / "Mesh M, Tri T" / "K overlapping tris") matches Python `_do_hover_xy` (lines 4351-4376)
- **Workspace intro status line** — on open, the status bar shows the same "{W}×{H} UV space · Click/drag to select · Scroll to zoom · Space+drag to pan · Ctrl+Z Undo · Ctrl+Shift+Z Redo · F11 fullscreen" hint as Python
- **Window-resize re-fit** — 50 ms debounced, 5 px delta threshold, fires only after the initial fit (Python `_on_window_resize`)
- **Scroll-zoom re-center** — once the scaled image fits entirely in the canvas, the view snaps to center instead of drifting with the cursor anchor
- **Undo/Redo toolbar buttons** — with enabled/disabled state tied to the per-stack emptiness; live alongside the existing Ctrl+Z / Ctrl+Shift+Z shortcuts
- **Sidebar selection counter** — "N island(s), M triangles" when anything is selected, "No triangles selected" otherwise (mirrors Python `sel_lbl` at 3441-3454)
- **Sidebar zoom controls** — `−` / `Fit` / `+` buttons plus a live zoom% label, wired to canvas zoom signals
- **Drag-marquee live island preview** — islands that intersect the drag rect are tinted with `surface_hi` (GPU renderer picks the preview color during VBO rebuild)
- **Info/Credits popup** — scrollable "Info" dialog with "What is this?", "Best For", "Limitations", "Workflow", "Controls", author link (Python `show_credits`, lines 2735-2838)
- **Island hover lockstep** — hovering a triangle on the canvas highlights its row in the sidebar; hovering a sidebar row paints the whole island on the canvas (whole-island hover, not just the triangle under the pointer)
- **Export validation** — "Nothing Selected" / "No Diffuse" warnings block exports with nothing to rasterize, mirroring Python `_do_export` (lines 4766-4773)
- **Smart export dialog** — save dialog is anchored in the exe directory and pre-filled with `{diffuse_stem}_cutout.{fmt}` (or `Diffuse_Cutouts.{fmt}` when no diffuse is loaded) — matches Python lines 4777-4791
- **Export status with path** — post-write status reads `Exported  ·  {path}` so the user can see exactly where the file landed (Python line 4805)
- **"No UV Data" warning** — replaces the generic "Empty mesh" message so mesh files with geometry but no UVs get the same wording as Python (line 3674)
- **Focus-out / leave cleanup** — the canvas flushes hover, drag, drag-preview, pan, and space-held state when focus is lost or the cursor leaves; sidebar highlights drop, hover label clears, status bar returns to the workspace intro (Python `_on_focus_out` lines 4423-4451 and `_on_leave` lines 4401-4421)
- **10 ms scroll-wheel throttle** — wheel events delivered inside a 10 ms window are dropped to keep zoom smooth on fast trackpads and high-resolution wheels (Python `_on_scroll` lines 4612-4616)
- **Themed sidebar scrollbar** — `QScrollBar` is restyled per-theme with a `surface_hi` rounded thumb on a `bg_panel` track and no step arrows, matching the custom Canvas-drawn scrollbar in Python (lines 3185-3213)
- **High-DPI canvas viewport** — the GL viewport is scaled by `devicePixelRatioF()` so the framebuffer matches the physical Qt surface. Fixes the top-of-canvas cutoff and cursor-to-highlight misalignment on 125 %/150 %/200 % Windows displays (pan/zoom math stays in logical pixels)
- **Zoom-to-fit uses actual UV size** — `zoomFit` now reads the loaded diffuse dimensions (falls back to 1024×1024 when no texture) and clamps zoom to ≤100 %, matching Python `_zoom_fit` (lines 4649-4668). Previously the fit ratio was computed against a hardcoded 1024, producing wrong scale and pan for non-1024 textures
- **Toggle-in-place selection** — click on an island flips its whole selection bit without clearing other selected islands (Python `_toggle_island_selection` lines 4120-4146). Drag-marquee release is now additive: every island whose UV-space bbox intersects the rectangle is set to selected, without wiping pre-existing selections (Python `_finish_drag_select` lines 4170-4201)
- **Workspace gating on first file** — loading a mesh OR a diffuse when neither the other asset nor the workspace are present keeps the user on the welcome screen with the "Open in Workspace" button exposed. The workspace is entered automatically only when the second asset arrives or the user clicks the button (Python `_load_mesh` 3706-3717 and `_load_diffuse` 3765-3769)
- **Theme contrast pass** — audited `parchment_faint` against `bg_panel`, `bg_toolbar`, and `bg_canvas` across all 34 themes. 33 themes had contrast below the 3.0:1 readable-dim-text threshold; each was HSL-lightened in place (hue and saturation preserved) to reach ≥3.5:1 minimum against every workspace/panel background. Affects every dim label that uses `parchment_faint`: welcome theme description, welcome status, welcome supports-strip, welcome info/credits links, sidebar selection counter, sidebar zoom% readout, sidebar scrollbar track, etc.
- **Theme source audit + identity pass** — diffed the C++ `resources/themes.json` against the Python `THEMES` dict and confirmed the port matched exactly aside from the intentional `parchment_faint` contrast lift above. Then selectively retuned six accent palettes whose UI read weaker than their names suggested while preserving the existing dark surfaces and text contrast work: `Akatosh` now leans sun-gold + sky blue, `Companions` weathered steel + leather, `Kynareth` airy sky + sage, `Stendarr` templar steel + holy gold, `Boethiah` obsidian-gold + blood red, and `Clavicus Vile` royal pact-blue + bargain gold. The per-island `sel_colors` arrays for those themes were updated to match the new accent families so multi-selection previews stay on-theme
- **Diffuse texture orientation** — removed a stray `.flipped(Qt::Vertical)` on upload in `GpuCanvasRenderer::upload_texture`. The background vertex shader places UV v=0 at the top of the canvas and the wireframe uses the same convention, so the texture has to be uploaded with QImage row 0 at texture memory 0 (unflipped). The flip was rendering every diffuse upside-down relative to the wireframe (and relative to the CPU renderer, which uses `QPainter::drawImage` — also unflipped — and was correct all along)
- **Fit-to-canvas on workspace entry** — `UVCanvasWidget::showEvent` now schedules a deferred `zoomFit()` (50 ms, after layout settles) whenever the canvas actually becomes visible, and `MainWindow::showWorkspace` adds a belt-and-suspenders refit for the case where the canvas is already the current stacked widget and just needs re-measurement. `setDiffuse` also refits when the loaded texture's size differs from the previous one, catching the texture-first-then-mesh load order that previously skipped the fit
- **Pixel-crisp zoom-in** — `QOpenGLTexture::setMagnificationFilter(Nearest)` so zooming in to inspect diffuse texels shows crisp squares instead of bilinear blur. Minification stays on Linear so fit-to-canvas downscaling remains smooth
- **Back-to-Home full reset** — `MainWindow::backToHome` now drops the scene completely: clears meshes via `setMeshes({})`, clears the diffuse via `setDiffuse(QImage())`, tears down the shape-tree rows, empties the undo / redo stacks (and their button states), clears the status bar + hover labels, resets the welcome screen's loaded-file state, forgets the cached diffuse path, and normalizes the alpha-checker toggle back to ON. Previously the welcome screen inherited the workspace-era status hint, a stale "Open in Workspace" button, live undo history, and a sticky alpha state from the previous session
- **Timer-after-backToHome guard** — the three `QTimer::singleShot` sites that fire `zoomFit` (canvas `showEvent`, canvas `resizeGL`, `MainWindow::showWorkspace`) each re-check `!meshes_.empty() || !diffuse_.isNull()` inside the lambda so a user who hits Back-to-Home during the 50 ms window doesn't trip a fit against an already-cleared canvas
- **ThemePickerDialog sizing** — the theme picker is now a fixed-size dialog with persistent `OK` / `Cancel`, live preview, stable category row sizing, and a consistently reserved scrollbar gutter so expanding categories does not resize the header rows

## Building

Requires:
- CMake ≥ 3.21
- Qt 6.5+ (verified 6.11.0)
- MSVC 2022 **or** MinGW-w64 (the mingw 13.1.0 bundled with Qt Tools works)
- Ninja (bundled under `C:\Qt\Tools\Ninja\` with the Qt installer)

### MinGW build (Qt-bundled toolchain, verified)

```bat
set PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;%PATH%
cmake -S "." -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.11.0\mingw_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
C:\Qt\6.11.0\mingw_64\bin\windeployqt.exe --release --no-translations "build\UV Cutout Tool.exe"
```

### MSVC build

```bat
cmake -S "." -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.11.0\msvc2022_64
cmake --build build --config Release
C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe "build\UV Cutout Tool.exe"
```

After build, the executable is `build\UV Cutout Tool.exe`. `NiflyDLL.dll` is
auto-copied from `..\pynifly_lib\io_scene_nifly\pyn\NiflyDLL.dll` if present.

### MinGW note

`app.rc` (the shell/taskbar icon resource) is only compiled under MSVC —
mingw's `windres.exe` mishandles include paths that contain spaces, and the
source tree lives under `UV Cutout Tool C++\`. The window icon still comes
from the Qt resource system (`:/icon.png`) on both toolchains; only the
.exe-embedded shell icon is skipped in mingw builds.

## Layout

```
src/
  codec/        BC decoders, DDS loader, TGA writer
  parsers/      OBJ parser, NIF parser (NiflyDLL wrapper)
  geometry/     Mesh data, spatial grid, point-in-triangle, island builder
  themes/       Theme struct + JSON loader
  render/       Canvas renderer interface, GPU backend, CPU backend
  ui/           Main window, welcome screen, OpenGL canvas widget, shape tree panel,
                theme picker dialog, warm button
shaders/        GLSL 330 core — background.vert/frag, wireframe.vert/frag
resources/      themes.json + theme_order.json (extracted from Python), icon.png/ico, resources.qrc
```
