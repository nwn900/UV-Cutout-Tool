#pragma once

#include <QColor>
#include <QPushButton>
#include <QString>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QEnterEvent;
class QMouseEvent;

namespace uvc::themes { struct Theme; }

namespace uvc::ui {

// Themed push-button with optional typed drag-and-drop filtering.
class WarmButton : public QPushButton {
    Q_OBJECT
public:
    enum Style { Primary, Secondary, Action, Welcome };
    enum DropKind { NoDrop, MeshDrop, DiffuseDrop };

    WarmButton(const QString& text, Style style, QWidget* parent = nullptr);

    void applyTheme(const themes::Theme& t);
    void setStyleKind(Style s) { style_ = s; repaintState(); }
    void setDropKind(DropKind kind);

signals:
    void fileDropped(const QString& path);

protected:
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void repaintState();
    Style style_;
    DropKind drop_kind_ = NoDrop;
    QColor bg_, bg_hi_, bg_act_, fg_;
    bool hovered_ = false;
    bool drag_hovered_ = false;
};

} // namespace uvc::ui
