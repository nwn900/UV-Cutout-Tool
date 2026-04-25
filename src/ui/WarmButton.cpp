#include "WarmButton.h"
#include "../themes/Theme.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>

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
    if (urls.size() != 1 || !urls.front().isLocalFile()) return {};
    return urls.front().toLocalFile();
}

bool accepts_for_kind(WarmButton::DropKind kind, const QString& path) {
    switch (kind) {
        case WarmButton::MeshDrop:    return is_mesh_file(path);
        case WarmButton::DiffuseDrop: return is_diffuse_file(path);
        case WarmButton::NoDrop:      return false;
    }
    return false;
}

} // namespace

WarmButton::WarmButton(const QString& text, Style style, QWidget* parent)
    : QPushButton(text, parent), style_(style) {
    setCursor(Qt::PointingHandCursor);
    setAutoFillBackground(true);
    setFlat(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(30);
}

void WarmButton::setDropKind(DropKind kind) {
    drop_kind_ = kind;
    setAcceptDrops(kind != NoDrop);
}

void WarmButton::applyTheme(const themes::Theme& t) {
    switch (style_) {
        case Primary:
            bg_ = t.primary;     bg_hi_ = t.primary_hi;  bg_act_ = t.primary_act; fg_ = t.parchment;
            break;
        case Secondary:
            bg_ = t.surface;     bg_hi_ = t.surface_hi;  bg_act_ = t.surface_act; fg_ = t.parchment_dim;
            break;
        case Action:
            bg_ = t.tertiary;    bg_hi_ = t.tertiary_txt; bg_act_ = t.tertiary_txt; fg_ = t.bg_toolbar;
            break;
        case Welcome:
            bg_ = t.bg_mid;      bg_hi_ = t.surface;     bg_act_ = t.surface_hi;  fg_ = t.secondary;
            break;
    }
    repaintState();
}

void WarmButton::repaintState() {
    const bool active_hover = hovered_ || drag_hovered_;
    const QColor bg = isDown() ? bg_act_ : (active_hover ? bg_hi_ : bg_);
    const int light_ratio = active_hover ? 120 : 100;
    const int dark_ratio = isDown() ? 85 : 115;
    QColor border = bg.lighter(light_ratio + 35);
    QColor shadow = bg.darker(dark_ratio + 55);
    QPalette p = palette();
    p.setColor(QPalette::Button, bg);
    p.setColor(QPalette::ButtonText, fg_);
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, fg_);
    setPalette(p);
    const QString padding = text().isEmpty() ? QStringLiteral("6px") : QStringLiteral("9px 16px");
    setStyleSheet(QString(
        "QPushButton {"
        " background:%1;"
        " color:%2;"
        " border:1px solid %3;"
        " border-bottom-color:%4;"
        " border-right-color:%4;"
        " border-radius:2px;"
        " padding:%5;"
        " font-weight:600;"
        "}"
        "QPushButton:focus { outline: none; border:1px solid %6; }")
        .arg(bg.name(QColor::HexArgb),
             fg_.name(QColor::HexArgb),
             border.name(QColor::HexArgb),
             shadow.name(QColor::HexArgb),
             padding,
             fg_.name(QColor::HexArgb)));
}

void WarmButton::enterEvent(QEnterEvent* e) {
    QPushButton::enterEvent(e);
    hovered_ = true;
    repaintState();
}
void WarmButton::leaveEvent(QEvent* e) {
    QPushButton::leaveEvent(e);
    hovered_ = false;
    repaintState();
}
void WarmButton::mousePressEvent(QMouseEvent* e) {
    QPushButton::mousePressEvent(e);
    repaintState();
}
void WarmButton::mouseReleaseEvent(QMouseEvent* e) {
    QPushButton::mouseReleaseEvent(e);
    repaintState();
}

void WarmButton::dragEnterEvent(QDragEnterEvent* e) {
    const QString path = dropped_local_file(e->mimeData());
    if (!path.isEmpty() && accepts_for_kind(drop_kind_, path)) {
        drag_hovered_ = true;
        repaintState();
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void WarmButton::dragMoveEvent(QDragMoveEvent* e) {
    const QString path = dropped_local_file(e->mimeData());
    if (!path.isEmpty() && accepts_for_kind(drop_kind_, path)) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void WarmButton::dragLeaveEvent(QDragLeaveEvent*) {
    drag_hovered_ = false;
    repaintState();
}

void WarmButton::dropEvent(QDropEvent* e) {
    const QString path = dropped_local_file(e->mimeData());
    drag_hovered_ = false;
    repaintState();
    if (!path.isEmpty() && accepts_for_kind(drop_kind_, path)) {
        emit fileDropped(path);
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

} // namespace uvc::ui
