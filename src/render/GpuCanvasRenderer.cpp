#include "GpuCanvasRenderer.h"

#include <QFile>
#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>
#include <QVector2D>
#include <QVector3D>

#include <cstring>
#include <unordered_map>

namespace uvc::render {

namespace {
QString load_shader(const char* path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

struct EdgeKey {
    // Quantized endpoints to dedupe near-coincident edges.
    int32_t a_u, a_v, b_u, b_v;
    bool operator==(const EdgeKey& o) const noexcept {
        return a_u == o.a_u && a_v == o.a_v && b_u == o.b_u && b_v == o.b_v;
    }
};
struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& k) const noexcept {
        std::size_t h = std::size_t(uint32_t(k.a_u));
        h = h * 1315423911u ^ std::size_t(uint32_t(k.a_v));
        h = h * 1315423911u ^ std::size_t(uint32_t(k.b_u));
        h = h * 1315423911u ^ std::size_t(uint32_t(k.b_v));
        return h;
    }
};

enum class EdgePriority : uint8_t {
    Base = 0,
    Hover = 1,
    Preview = 2,
    Selected = 3,
};
} // namespace

GpuCanvasRenderer::GpuCanvasRenderer() = default;
GpuCanvasRenderer::~GpuCanvasRenderer() = default;

bool GpuCanvasRenderer::initializeGL() {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) return false;
    gl_ = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
    if (!gl_ || !gl_->initializeOpenGLFunctions()) return false;

    bg_prog_ = std::make_unique<QOpenGLShaderProgram>();
    bg_prog_->addShaderFromSourceCode(QOpenGLShader::Vertex,   load_shader(":/shaders/background.vert"));
    bg_prog_->addShaderFromSourceCode(QOpenGLShader::Fragment, load_shader(":/shaders/background.frag"));
    if (!bg_prog_->link()) return false;

    wire_prog_ = std::make_unique<QOpenGLShaderProgram>();
    wire_prog_->addShaderFromSourceCode(QOpenGLShader::Vertex,   load_shader(":/shaders/wireframe.vert"));
    wire_prog_->addShaderFromSourceCode(QOpenGLShader::Fragment, load_shader(":/shaders/wireframe.frag"));
    if (!wire_prog_->link()) return false;

    // Background quad: two triangles covering [0,1] with matching UVs.
    struct BgV { float px, py, u, v; };
    BgV quad[6] = {
        {0.f, 0.f, 0.f, 0.f}, {1.f, 0.f, 1.f, 0.f}, {1.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 0.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 1.f, 0.f, 1.f},
    };
    bg_vao_.create();
    bg_vao_.bind();
    bg_vbo_.create();
    bg_vbo_.bind();
    bg_vbo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
    bg_vbo_.allocate(quad, sizeof(quad));
    gl_->glEnableVertexAttribArray(0);
    gl_->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BgV), (void*)0);
    gl_->glEnableVertexAttribArray(1);
    gl_->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BgV), (void*)(sizeof(float) * 2));
    bg_vbo_.release();
    bg_vao_.release();

    wire_vao_.create();
    wire_vao_.bind();
    wire_vbo_.create();
    wire_vbo_.bind();
    wire_vbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    // Two attributes: vec2 uv + vec4 color → 6 floats / vertex. Size TBD at rebuild time.
    gl_->glEnableVertexAttribArray(0);
    gl_->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
    gl_->glEnableVertexAttribArray(1);
    gl_->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    wire_vbo_.release();
    wire_vao_.release();

    gl_ready_ = true;
    return true;
}

void GpuCanvasRenderer::releaseGL() {
    if (!gl_ready_) return;
    bg_vbo_.destroy();
    bg_vao_.destroy();
    wire_vbo_.destroy();
    wire_vao_.destroy();
    bg_prog_.reset();
    wire_prog_.reset();
    release_texture();
    gl_ready_ = false;
}

void GpuCanvasRenderer::upload_texture(const QImage& img) {
    release_texture();
    if (img.isNull()) return;
    // IMPORTANT: upload the QImage row 0 → texture row 0 as-is. Our background
    // vertex shader places UV v=0 at the TOP of the canvas (it flips NDC.y
    // after the divide), and the wireframe also treats mesh-UV v=0 as top.
    // Sampling at v_uv=0 therefore has to return the top of the picture, i.e.
    // row 0 of the QImage. A prior `.flipped(Qt::Vertical)` here put the
    // picture's bottom row at texture memory 0, which rendered the whole
    // diffuse upside-down relative to the wireframe and the CPU renderer
    // (which uses QPainter::drawImage — also no flip).
    tex_ = std::make_unique<QOpenGLTexture>(img.convertToFormat(QImage::Format_RGBA8888));
    // Minification uses linear so downscaling (small zoom / fit-to-canvas on
    // high-res textures) stays smooth. Magnification uses NEAREST so the user
    // can zoom in and see crisp texels — a pixel-art preview. Bilinear
    // magnification would blur each texel into its neighbors, which defeats
    // the whole point of zooming in to inspect diffuse pixels.
    tex_->setMinificationFilter(QOpenGLTexture::Linear);
    tex_->setMagnificationFilter(QOpenGLTexture::Nearest);
    tex_->setWrapMode(QOpenGLTexture::ClampToEdge);
    has_texture_ = true;
}

void GpuCanvasRenderer::release_texture() {
    tex_.reset();
    has_texture_ = false;
}

void GpuCanvasRenderer::onTextureChanged(const QImage* img) {
    if (!gl_ready_) return;
    if (img && !img->isNull()) upload_texture(*img);
    else                       release_texture();
}

void GpuCanvasRenderer::onMeshesChanged(const std::vector<geom::Mesh>& meshes, const SceneState& scene) {
    if (!gl_ready_) return;
    rebuild_line_vertices(meshes, scene);
}

void GpuCanvasRenderer::onSelectionChanged(const std::vector<geom::Mesh>& meshes, const SceneState& scene) {
    if (!gl_ready_) return;
    rebuild_line_vertices(meshes, scene);
}

void GpuCanvasRenderer::rebuild_line_vertices(const std::vector<geom::Mesh>& meshes, const SceneState& scene) {
    // One vertex (uv + rgba) per line endpoint. For each triangle we emit 3 edges = 6 vertices,
    // then dedupe shared edges using a hash set.
    const float wr = scene.wire_color.redF();
    const float wg = scene.wire_color.greenF();
    const float wb = scene.wire_color.blueF();
    const float hr = scene.hover_color.redF();
    const float hg = scene.hover_color.greenF();
    const float hb = scene.hover_color.blueF();
    const float sr = scene.selected_color.redF();
    const float sg = scene.selected_color.greenF();
    const float sb = scene.selected_color.blueF();
    const float pr = scene.preview_color.redF();
    const float pg = scene.preview_color.greenF();
    const float pb = scene.preview_color.blueF();

    std::vector<float> verts;
    verts.reserve(meshes.size() * 1024);

    auto quantize = [](float x) -> int32_t {
        return int32_t(std::lround(double(x) * 1e6));
    };

    std::unordered_map<EdgeKey, int, EdgeKeyHash> seen;
    std::unordered_map<EdgeKey, EdgePriority, EdgeKeyHash> priorities;

    for (size_t mi = 0; mi < meshes.size(); ++mi) {
        const auto& m = meshes[mi];
        if (!m.visible) continue;
        seen.clear();
        priorities.clear();
        for (size_t ti = 0; ti < m.triangles.size(); ++ti) {
            const auto& t = m.triangles[ti];
            bool hover = (int(mi) == scene.hover_mesh_idx && int(ti) == scene.hover_tri_idx);
            // Whole-island hover: tint every triangle whose (mesh, island)
            // matches the hover target.
            if (!hover && scene.hover_island_idx >= 0 &&
                int(mi) == scene.hover_island_mesh_idx &&
                t.island_id.has_value() &&
                *t.island_id == scene.hover_island_idx) {
                hover = true;
            }
            bool preview = false;
            if (!scene.drag_preview_islands.empty() && t.island_id.has_value()) {
                uint64_t key = (uint64_t(mi) << 32) | uint32_t(*t.island_id);
                preview = scene.drag_preview_islands.count(key) != 0;
            }
            for (int e = 0; e < 3; ++e) {
                auto a = t.uv[e];
                auto b = t.uv[(e + 1) % 3];
                // Order endpoints canonically.
                bool swap = std::tie(a.u, a.v) > std::tie(b.u, b.v);
                auto p1 = swap ? b : a;
                auto p2 = swap ? a : b;
                EdgeKey key{ quantize(p1.u), quantize(p1.v), quantize(p2.u), quantize(p2.v) };
                float r, g, bl;
                EdgePriority priority = EdgePriority::Base;
                if (t.selected)      { r = sr; g = sg; bl = sb; priority = EdgePriority::Selected; }
                else if (preview)    { r = pr; g = pg; bl = pb; priority = EdgePriority::Preview; }
                else if (hover)      { r = hr; g = hg; bl = hb; priority = EdgePriority::Hover; }
                else                 { r = wr; g = wg; bl = wb; }
                auto it = seen.find(key);
                if (it == seen.end()) {
                    seen.emplace(key, int(verts.size() / 6));
                    priorities.emplace(key, priority);
                    verts.insert(verts.end(), { p1.u, p1.v, r, g, bl, 1.f });
                    verts.insert(verts.end(), { p2.u, p2.v, r, g, bl, 1.f });
                } else {
                    int base = it->second * 6;
                    const auto current_priority = priorities[key];
                    if (priority > current_priority) {
                        priorities[key] = priority;
                        verts[base + 2] = r; verts[base + 3] = g; verts[base + 4] = bl;
                        verts[base + 8] = r; verts[base + 9] = g; verts[base + 10] = bl;
                    }
                }
            }
        }
    }

    wire_vertex_count_ = int(verts.size() / 6);
    wire_vbo_.bind();
    wire_vbo_.allocate(verts.data(), int(verts.size() * sizeof(float)));
    wire_vbo_.release();
}

void GpuCanvasRenderer::render(const CanvasView& view, const SceneState& scene) {
    if (!gl_ready_) return;
    // Viewport is in PHYSICAL framebuffer pixels (QOpenGLWidget allocates the
    // framebuffer at `width() * devicePixelRatio()`). Without the dpr factor
    // the viewport lands in the bottom-left quadrant on high-DPI displays and
    // the top/right of the canvas shows the clear color — appearing as a
    // "large section cut off at the top". All shader math works in the
    // logical canvas size we pass via `u_canvas`.
    const int vw = int(std::lround(view.canvas_pixels.width()  * double(view.device_pixel_ratio)));
    const int vh = int(std::lround(view.canvas_pixels.height() * double(view.device_pixel_ratio)));
    gl_->glViewport(0, 0, vw, vh);
    gl_->glClearColor(scene.bg_canvas_color.redF(),
                      scene.bg_canvas_color.greenF(),
                      scene.bg_canvas_color.blueF(),
                      1.f);
    gl_->glClear(GL_COLOR_BUFFER_BIT);

    const float uvw = scene.uv_size.width()  * view.zoom;
    const float uvh = scene.uv_size.height() * view.zoom;

    // ─── Background textured quad ─────────────────────────────────────────────
    bg_prog_->bind();
    bg_prog_->setUniformValue("u_canvas",   QVector2D(view.canvas_pixels.width(), view.canvas_pixels.height()));
    bg_prog_->setUniformValue("u_uv_origin",QVector2D(view.pan_x, view.pan_y));
    bg_prog_->setUniformValue("u_uv_size",  QVector2D(uvw, uvh));
    bg_prog_->setUniformValue("u_has_tex",  has_texture_);
    bg_prog_->setUniformValue("u_alpha_on", view.alpha_on && view.content_supports_alpha);
    bg_prog_->setUniformValue("u_bg_color",
                              QVector3D(scene.bg_canvas_color.redF(),
                                        scene.bg_canvas_color.greenF(),
                                        scene.bg_canvas_color.blueF()));
    bg_prog_->setUniformValue("u_checker_dark",
                              QVector3D(scene.checker_dark.redF(),
                                        scene.checker_dark.greenF(),
                                        scene.checker_dark.blueF()));
    bg_prog_->setUniformValue("u_checker_light",
                              QVector3D(scene.checker_light.redF(),
                                        scene.checker_light.greenF(),
                                        scene.checker_light.blueF()));
    if (has_texture_ && tex_) {
        gl_->glActiveTexture(GL_TEXTURE0);
        tex_->bind();
        bg_prog_->setUniformValue("u_tex", 0);
    }
    bg_vao_.bind();
    gl_->glDrawArrays(GL_TRIANGLES, 0, 6);
    bg_vao_.release();
    bg_prog_->release();

    // ─── Wireframe: ONE draw call for every visible edge on every mesh ────────
    if (wire_vertex_count_ > 0) {
        wire_prog_->bind();
        wire_prog_->setUniformValue("u_canvas",    QVector2D(view.canvas_pixels.width(), view.canvas_pixels.height()));
        wire_prog_->setUniformValue("u_uv_origin", QVector2D(view.pan_x, view.pan_y));
        wire_prog_->setUniformValue("u_uv_size",   QVector2D(uvw, uvh));
        wire_vao_.bind();
        gl_->glLineWidth(1.0f);
        gl_->glDrawArrays(GL_LINES, 0, wire_vertex_count_);
        wire_vao_.release();
        wire_prog_->release();
    }
}

} // namespace uvc::render
