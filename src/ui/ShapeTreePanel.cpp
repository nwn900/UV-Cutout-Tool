#include "ShapeTreePanel.h"

#include "../geometry/MeshData.h"
#include "../themes/Theme.h"

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpacerItem>

namespace uvc::ui {

namespace {
// Compact roles: store mesh_idx / island_idx on the QLabel as dynamic properties
// so the event filter can recover them generically.
constexpr const char* kMeshIdxProp    = "uvc.mesh_idx";
constexpr const char* kIslandIdxProp  = "uvc.island_idx";
constexpr const char* kRoleProp       = "uvc.role"; // "mesh_cb", "mesh_lbl", "island_lbl", "zoom_out", "zoom_fit", "zoom_in"

QLabel* makeClickableLabel(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setObjectName("shapeTreeLabel");
    l->setAutoFillBackground(true);
    l->setCursor(Qt::PointingHandCursor);
    l->setMargin(2);
    l->setMouseTracking(true);
    return l;
}
} // namespace

ShapeTreePanel::ShapeTreePanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    header_ = new QLabel("Shapes", this);
    header_->setMargin(6);
    QFont hf = header_->font();
    hf.setBold(true);
    header_->setFont(hf);
    outer->addWidget(header_);

    status_ = new QLabel("No mesh loaded", this);
    status_->setMargin(4);
    outer->addWidget(status_);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFrameShape(QFrame::NoFrame);

    body_ = new QWidget(scroll_);
    vlay_ = new QVBoxLayout(body_);
    vlay_->setContentsMargins(4, 2, 4, 8);
    vlay_->setSpacing(1);
    vlay_->addStretch();
    scroll_->setWidget(body_);

    outer->addWidget(scroll_, 1);

    // Footer: zoom controls row on top, selection counter below.
    footer_ = new QWidget(this);
    footer_->setObjectName("shapeFooter");
    auto* fv = new QVBoxLayout(footer_);
    fv->setContentsMargins(6, 6, 6, 6);
    fv->setSpacing(4);

    auto* zrow = new QHBoxLayout();
    zrow->setSpacing(4);
    auto make_zoom_btn = [&](const QString& glyph, const char* role) {
        auto* l = new QLabel(glyph, footer_);
        l->setAlignment(Qt::AlignCenter);
        l->setAutoFillBackground(true);
        l->setCursor(Qt::PointingHandCursor);
        l->setFixedHeight(22);
        l->setMinimumWidth(28);
        l->setProperty(kRoleProp, role);
        l->setProperty("uvc.hovered", false);
        l->setProperty("uvc.pressed", false);
        l->installEventFilter(this);
        return l;
    };
    zoom_out_btn_ = make_zoom_btn("\u2212", "zoom_out");  // minus
    zoom_fit_btn_ = make_zoom_btn("Fit",     "zoom_fit");
    zoom_in_btn_  = make_zoom_btn("+",       "zoom_in");
    zoom_pct_lbl_ = new QLabel("100%", footer_);
    zoom_pct_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont zf = zoom_pct_lbl_->font();
    zf.setPointSize(8);
    zoom_pct_lbl_->setFont(zf);
    zoom_pct_lbl_->setMinimumWidth(44);

    zrow->addWidget(zoom_out_btn_);
    zrow->addWidget(zoom_fit_btn_);
    zrow->addWidget(zoom_in_btn_);
    zrow->addStretch();
    zrow->addWidget(zoom_pct_lbl_);
    fv->addLayout(zrow);

    sel_lbl_ = new QLabel("No triangles selected", footer_);
    QFont sf = sel_lbl_->font();
    sf.setItalic(true);
    sf.setPointSize(8);
    sel_lbl_->setFont(sf);
    sel_lbl_->setWordWrap(true);
    fv->addWidget(sel_lbl_);

    outer->addWidget(footer_);

    setMinimumWidth(220);
}

void ShapeTreePanel::updateSummaryText(int mesh_count, int island_count) {
    if (!header_ || !status_) return;

    const QString mesh_word = (mesh_count == 1) ? "Shape" : "Shapes";
    header_->setText(mesh_word);

    if (mesh_count <= 0) {
        status_->setText("No mesh loaded");
        return;
    }

    const QString mesh_label = QString("%1 %2")
        .arg(mesh_count)
        .arg(mesh_count == 1 ? "shape" : "shapes");
    const QString island_label = QString("%1 %2")
        .arg(island_count)
        .arg(island_count == 1 ? "island" : "islands");
    status_->setText(QString("%1, %2").arg(mesh_label, island_label));
}

void ShapeTreePanel::clear_rows() {
    for (auto& r : mesh_rows_)   if (r.row) r.row->deleteLater();
    for (auto& r : island_rows_) if (r.row) r.row->deleteLater();
    mesh_rows_.clear();
    island_rows_.clear();
}

void ShapeTreePanel::apply_row_palette(QWidget* w, const QColor& bg) {
    if (!w) return;
    QPalette p = w->palette();
    p.setColor(QPalette::Window, bg);
    w->setPalette(p);
    w->setAutoFillBackground(true);
    if (theme_) {
        const QString selector = qobject_cast<QLabel*>(w)
            ? QStringLiteral("QLabel#shapeTreeLabel")
            : QStringLiteral("QWidget#shapeTreeRow");
        w->setStyleSheet(QString(
            "%1 { background:%2; border-bottom:1px solid %3; }")
            .arg(selector,
                 bg.name(QColor::HexRgb),
                 theme_->rule_dim.name(QColor::HexRgb)));
    }
}

void ShapeTreePanel::paint_checkbox(QLabel* cb, bool checked) {
    if (!cb || !theme_) return;
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor border = theme_->rule;
    QColor fill   = checked ? theme_->primary : theme_->bg_panel;
    p.setPen(border);
    p.setBrush(fill);
    p.drawRoundedRect(0, 0, 13, 13, 2, 2);
    if (checked) {
        p.setPen(QPen(theme_->parchment, 2));
        p.drawLine(3, 7, 6, 10);
        p.drawLine(6, 10, 11, 3);
    }
    cb->setPixmap(pm);
}

void ShapeTreePanel::style_zoom_button(QLabel* label, bool hovered, bool pressed) {
    if (!label || !theme_) return;
    const QColor base = pressed ? theme_->surface_act
                      : hovered ? theme_->surface_hi
                                : theme_->surface;
    const QColor top = base.lighter(pressed ? 105 : 128);
    const QColor bottom = base.darker(pressed ? 138 : 118);
    label->setStyleSheet(QString(
        "QLabel {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2);"
        " color:%3;"
        " border:1px solid %4;"
        " border-bottom-color:%5;"
        " padding:2px 6px;"
        " font-weight:600;"
        "}")
        .arg(top.name(QColor::HexRgb),
             bottom.name(QColor::HexRgb),
             theme_->parchment_dim.name(QColor::HexRgb),
             theme_->rule.name(QColor::HexRgb),
             theme_->rule_dim.name(QColor::HexRgb)));
}

void ShapeTreePanel::rebuild(const std::vector<geom::Mesh>& meshes) {
    clear_rows();

    // Remove the trailing stretch, we'll re-add it.
    if (vlay_->count() > 0) {
        auto* item = vlay_->takeAt(vlay_->count() - 1);
        if (item) { if (!item->widget()) delete item; }
    }

    int total_islands = 0;
    int global_idx = 1;

    for (int mi = 0; mi < int(meshes.size()); ++mi) {
        const auto& m = meshes[mi];

        MeshRow mr;
        mr.mesh_idx = mi;
        mr.row = new QWidget(body_);
        mr.row->setObjectName("shapeTreeRow");
        auto* hl = new QHBoxLayout(mr.row);
        hl->setContentsMargins(4, 2, 4, 2);
        hl->setSpacing(6);

        mr.checkbox = new QLabel(mr.row);
        mr.checkbox->setFixedSize(16, 16);
        mr.checkbox->setCursor(Qt::PointingHandCursor);
        mr.checkbox->setProperty(kMeshIdxProp, mi);
        mr.checkbox->setProperty(kRoleProp, "mesh_cb");
        mr.checkbox->installEventFilter(this);
        hl->addWidget(mr.checkbox);

        const QString mesh_label = m.name.empty()
            ? QString("Shape %1").arg(mi + 1)
            : QString::fromStdString(m.name);
        mr.name_lbl = makeClickableLabel(mesh_label, mr.row);
        QFont nf = mr.name_lbl->font();
        nf.setBold(true);
        mr.name_lbl->setFont(nf);
        mr.name_lbl->setProperty(kMeshIdxProp, mi);
        mr.name_lbl->setProperty(kRoleProp, "mesh_lbl");
        mr.name_lbl->installEventFilter(this);
        hl->addWidget(mr.name_lbl, 1);

        vlay_->addWidget(mr.row);
        mesh_rows_.push_back(mr);
        paint_checkbox(mr.checkbox, m.visible);

        for (int ii = 0; ii < int(m.islands.size()); ++ii) {
            IslandRow ir;
            ir.mesh_idx = mi;
            ir.island_idx = ii;
            ir.row = new QWidget(body_);
            ir.row->setObjectName("shapeTreeRow");
            auto* ihl = new QHBoxLayout(ir.row);
            ihl->setContentsMargins(4, 1, 4, 1);
            ihl->setSpacing(6);

            auto* spacer = new QWidget(ir.row);
            spacer->setFixedWidth(20);
            ihl->addWidget(spacer);

            ir.name_lbl = makeClickableLabel(
                QString("UV Island %1").arg(global_idx), ir.row);
            ir.name_lbl->setProperty(kMeshIdxProp, mi);
            ir.name_lbl->setProperty(kIslandIdxProp, ii);
            ir.name_lbl->setProperty(kRoleProp, "island_lbl");
            ir.name_lbl->installEventFilter(this);
            ihl->addWidget(ir.name_lbl, 1);

            vlay_->addWidget(ir.row);
            island_rows_.push_back(ir);
            ++global_idx;
            ++total_islands;
        }
    }

    vlay_->addStretch();

    updateSummaryText(int(meshes.size()), total_islands);

    refreshHighlights(meshes);
}

void ShapeTreePanel::refreshHighlights(const std::vector<geom::Mesh>& meshes,
                                       int hover_mesh_idx,
                                       int hover_island_idx) {
    refreshSelectionCounter(meshes);
    if (!theme_) return;

    for (auto& mr : mesh_rows_) {
        if (mr.mesh_idx < 0 || mr.mesh_idx >= int(meshes.size())) continue;
        const auto& m = meshes[mr.mesh_idx];
        bool any_sel = false, all_sel = !m.triangles.empty();
        for (const auto& t : m.triangles) {
            if (t.selected) any_sel = true; else all_sel = false;
            if (any_sel && !all_sel) break;
        }
        QColor bg;
        QColor fg = theme_->parchment;
        if (all_sel)       bg = theme_->primary;
        else if (any_sel)  bg = theme_->surface_act;
        else               bg = theme_->surface;
        apply_row_palette(mr.row,      bg);
        apply_row_palette(mr.name_lbl, bg);
        QPalette lp = mr.name_lbl->palette();
        lp.setColor(QPalette::WindowText, fg);
        mr.name_lbl->setPalette(lp);
        paint_checkbox(mr.checkbox, m.visible);
    }

    for (auto& ir : island_rows_) {
        if (ir.mesh_idx < 0 || ir.mesh_idx >= int(meshes.size())) continue;
        const auto& m = meshes[ir.mesh_idx];
        bool any_sel = false;
        for (const auto& t : m.triangles) {
            if (t.island_id.has_value() && *t.island_id == ir.island_idx && t.selected) {
                any_sel = true;
                break;
            }
        }
        const bool hovered = (ir.mesh_idx == hover_mesh_idx &&
                              ir.island_idx == hover_island_idx);
        QColor bg;
        if (any_sel && hovered)      bg = theme_->primary_hi;
        else if (any_sel)            bg = theme_->primary;
        else if (hovered)            bg = theme_->surface_hi;
        else                         bg = theme_->bg_panel;
        apply_row_palette(ir.row,      bg);
        apply_row_palette(ir.name_lbl, bg);
        QPalette lp = ir.name_lbl->palette();
        lp.setColor(QPalette::WindowText, any_sel ? theme_->parchment : theme_->parchment_dim);
        ir.name_lbl->setPalette(lp);
    }
}

void ShapeTreePanel::refreshSelectionCounter(const std::vector<geom::Mesh>& meshes) {
    if (!sel_lbl_) return;
    int total_sel = 0;
    // Deduplicate islands by mesh pointer and island index so identical island
    // numbers in different meshes stay distinct.
    std::vector<std::pair<const geom::Mesh*, int>> selected_islands;
    for (const auto& m : meshes) {
        for (const auto& t : m.triangles) {
            if (!t.selected) continue;
            ++total_sel;
            if (t.island_id.has_value()) {
                auto key = std::make_pair(&m, *t.island_id);
                bool seen = false;
                for (const auto& k : selected_islands)
                    if (k.first == key.first && k.second == key.second) { seen = true; break; }
                if (!seen) selected_islands.push_back(key);
            }
        }
    }
    if (total_sel == 0) {
        sel_lbl_->setText("No triangles selected");
        if (theme_) {
            QPalette p = sel_lbl_->palette();
            p.setColor(QPalette::WindowText, theme_->parchment_faint);
            sel_lbl_->setPalette(p);
        }
    } else {
        const int ni = int(selected_islands.size());
        sel_lbl_->setText(QString("%1 island%2, %3 triangles")
                              .arg(ni)
                              .arg(ni == 1 ? "" : "s")
                              .arg(total_sel));
        if (theme_) {
            QPalette p = sel_lbl_->palette();
            p.setColor(QPalette::WindowText, theme_->parchment_dim);
            sel_lbl_->setPalette(p);
        }
    }
}

void ShapeTreePanel::setZoomPercent(int pct) {
    if (zoom_pct_lbl_) zoom_pct_lbl_->setText(QString("%1%").arg(pct));
}

void ShapeTreePanel::applyTheme(const themes::Theme& t) {
    theme_ = &t;
    QPalette p = palette();
    p.setColor(QPalette::Window,     t.bg_panel);
    p.setColor(QPalette::WindowText, t.parchment);
    setPalette(p);
    setAutoFillBackground(true);
    setStyleSheet(QString(
        "ShapeTreePanel {"
        " background:%1;"
        " border-left:1px solid %2;"
        "}")
        .arg(t.bg_panel.name(QColor::HexRgb),
             t.rule.name(QColor::HexRgb)));

    auto style_side = [&](QLabel* l, const QColor& bg, const QColor& fg) {
        QPalette pp = l->palette();
        pp.setColor(QPalette::Window,     bg);
        pp.setColor(QPalette::WindowText, fg);
        l->setPalette(pp);
        l->setAutoFillBackground(true);
    };
    style_side(header_, t.bg_toolbar,  t.parchment);
    style_side(status_, t.bg_panel,    t.parchment_faint);
    if (header_) {
        header_->setStyleSheet(QString(
            "QLabel {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2);"
            " color:%3;"
            " border-bottom:1px solid %4;"
            " padding:8px 10px;"
            "}")
            .arg(t.surface.name(QColor::HexRgb),
                 t.bg_toolbar.name(QColor::HexRgb),
                 t.parchment.name(QColor::HexRgb),
                 t.rule.name(QColor::HexRgb)));
    }
    if (status_) {
        status_->setStyleSheet(QString(
            "QLabel { color:%1; border-bottom:1px solid %2; padding:5px 10px; }")
            .arg(t.parchment_faint.name(QColor::HexRgb),
                 t.rule_dim.name(QColor::HexRgb)));
    }

    if (footer_) {
        QPalette fp = footer_->palette();
        fp.setColor(QPalette::Window, t.bg_toolbar);
        footer_->setPalette(fp);
        footer_->setAutoFillBackground(true);
        footer_->setStyleSheet(QString(
            "QWidget#shapeFooter { background:%1; border-top:1px solid %2; }")
            .arg(t.bg_toolbar.name(QColor::HexRgb),
                 t.rule.name(QColor::HexRgb)));
    }
    for (auto* b : {zoom_out_btn_, zoom_fit_btn_, zoom_in_btn_}) {
        if (!b) continue;
        QPalette bp = b->palette();
        bp.setColor(QPalette::Window,     t.surface);
        bp.setColor(QPalette::WindowText, t.parchment_dim);
        b->setPalette(bp);
        style_zoom_button(b, b->property("uvc.hovered").toBool(), b->property("uvc.pressed").toBool());
    }
    if (zoom_pct_lbl_) style_side(zoom_pct_lbl_, t.bg_toolbar, t.parchment_dim);
    if (sel_lbl_)      style_side(sel_lbl_,      t.bg_toolbar, t.parchment_faint);

    if (body_) {
        QPalette bp = body_->palette();
        bp.setColor(QPalette::Window, t.bg_panel);
        body_->setPalette(bp);
        body_->setAutoFillBackground(true);
    }
    if (scroll_ && scroll_->viewport()) {
        QPalette sp = scroll_->viewport()->palette();
        sp.setColor(QPalette::Window, t.bg_panel);
        scroll_->viewport()->setPalette(sp);
        scroll_->viewport()->setAutoFillBackground(true);

        // Themed scrollbar with a 14px track, rounded thumb, and no step arrows.
        const QString panel = t.bg_panel.name(QColor::HexRgb);
        const QString thumb = t.surface_hi.name(QColor::HexRgb);
        const QString qss = QString(
            "QScrollBar:vertical { background: %1; width: 14px; margin: 0; border-left:1px solid %3; }"
            "QScrollBar::handle:vertical { background: %2; border:1px solid %4; border-radius: 2px; min-height: 20px; margin: 2px 3px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; background: none; border: none; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        ).arg(panel, thumb, t.rule_dim.name(QColor::HexRgb), t.rule.name(QColor::HexRgb));
        scroll_->verticalScrollBar()->setStyleSheet(qss);
    }

    // Row backgrounds and checkbox art are repainted by refreshHighlights(); the
    // owner is expected to call it after applyTheme() when meshes are loaded.
}

bool ShapeTreePanel::eventFilter(QObject* obj, QEvent* e) {
    const QString role = obj->property(kRoleProp).toString();
    const bool is_zoom =
        role == "zoom_out" || role == "zoom_fit" || role == "zoom_in";
    if (is_zoom) {
        auto* label = qobject_cast<QLabel*>(obj);
        if (e->type() == QEvent::Enter) {
            obj->setProperty("uvc.hovered", true);
            style_zoom_button(label, true, obj->property("uvc.pressed").toBool());
            return false;
        }
        if (e->type() == QEvent::Leave) {
            obj->setProperty("uvc.hovered", false);
            obj->setProperty("uvc.pressed", false);
            style_zoom_button(label, false, false);
            return false;
        }
        if (e->type() == QEvent::MouseButtonPress) {
            obj->setProperty("uvc.pressed", true);
            style_zoom_button(label, obj->property("uvc.hovered").toBool(), true);
            return false;
        }
        if (e->type() == QEvent::MouseButtonRelease) {
            obj->setProperty("uvc.pressed", false);
            style_zoom_button(label, obj->property("uvc.hovered").toBool(), false);
        }
    }

    // Hover lockstep: fire hover-change signals when the pointer enters or
    // leaves an island row label.
    if (e->type() == QEvent::Enter || e->type() == QEvent::Leave) {
        if (role == "island_lbl") {
            if (e->type() == QEvent::Enter) {
                const int mi = obj->property(kMeshIdxProp).toInt();
                const int ii = obj->property(kIslandIdxProp).toInt();
                emit islandRowHoverChanged(mi, ii);
            } else {
                emit islandRowHoverChanged(-1, -1);
            }
        }
        return QWidget::eventFilter(obj, e);
    }

    if (e->type() != QEvent::MouseButtonRelease) return QWidget::eventFilter(obj, e);
    auto* me = static_cast<QMouseEvent*>(e);
    if (me->button() != Qt::LeftButton) return false;

    const int mi = obj->property(kMeshIdxProp).toInt();

    if (role == "mesh_cb") {
        emit meshVisibilityToggleRequested(mi);
        return true;
    }
    if (role == "mesh_lbl") {
        emit meshSelectionToggleRequested(mi);
        return true;
    }
    if (role == "island_lbl") {
        const int ii = obj->property(kIslandIdxProp).toInt();
        emit islandSelectionToggleRequested(mi, ii);
        return true;
    }
    if (role == "zoom_out") { emit zoomOutRequested(); return true; }
    if (role == "zoom_fit") { emit zoomFitRequested(); return true; }
    if (role == "zoom_in")  { emit zoomInRequested();  return true; }
    return false;
}

} // namespace uvc::ui
