#include "ThemePickerDialog.h"

#include "../themes/Theme.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>

namespace uvc::ui {

namespace {
constexpr const char* kCatIdxProp   = "uvc.cat_idx";
constexpr const char* kThemeNameProp = "uvc.theme_name";
constexpr const char* kRoleProp     = "uvc.role";
constexpr const char* kHoveredProp  = "uvc.hovered";
constexpr const char* kRowHeightProp = "uvc.row_height";
} // namespace

ThemePickerDialog::ThemePickerDialog(QWidget* parent)
    : QDialog(parent, Qt::Dialog) {
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle("Themes");
    resize(640, 540);
    setMinimumSize(620, 520);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 10);
    outer->setSpacing(0);

    title_ = new QLabel("Choose Theme", this);
    title_->setAlignment(Qt::AlignCenter);
    title_->setAutoFillBackground(true);
    title_->setMargin(8);
    QFont tf = title_->font();
    tf.setBold(true);
    tf.setPointSize(10);
    title_->setFont(tf);
    outer->addWidget(title_);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scroll_->setFrameShape(QFrame::NoFrame);

    body_ = new QWidget(scroll_);
    body_layout_ = new QVBoxLayout(body_);
    body_layout_->setContentsMargins(0, 0, 0, 0);
    body_layout_->setSpacing(0);
    scroll_->setWidget(body_);

    outer->addWidget(scroll_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_btn_ = buttons->button(QDialogButtonBox::Ok);
    cancel_btn_ = buttons->button(QDialogButtonBox::Cancel);
    outer->addSpacing(10);
    outer->addWidget(buttons);

    connect(ok_btn_, &QPushButton::clicked, this, [this] {
        emit themeSelected(pending_theme_name_);
        hide();
    });
    connect(cancel_btn_, &QPushButton::clicked, this, [this] {
        emit themeSelectionCanceled(original_theme_name_);
        hide();
    });

    build_rows();
    updateButtonState();
}

void ThemePickerDialog::build_rows() {
    const auto& mgr = themes::ThemeManager::instance();
    const auto& cats = mgr.categoryOrder();

    for (int ci = 0; ci < cats.size(); ++ci) {
        Category c;
        c.name = cats[ci].first;

        c.header = new QLabel(QString("> %1").arg(c.name), body_);
        c.header->setAutoFillBackground(true);
        c.header->setCursor(Qt::PointingHandCursor);
        c.header->setMargin(0);
        c.header->setIndent(12);
        c.header->setMinimumHeight(36);
        c.header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QFont hf = c.header->font();
        hf.setBold(true);
        c.header->setFont(hf);
        c.header->setProperty(kCatIdxProp, ci);
        c.header->setProperty(kRoleProp, "cat_header");
        c.header->installEventFilter(this);
        body_layout_->addWidget(c.header);

        c.body = new QWidget(body_);
        auto* bl = new QVBoxLayout(c.body);
        bl->setContentsMargins(0, 0, 0, 4);
        bl->setSpacing(0);
        for (const QString& tname : cats[ci].second) {
            const auto& th = mgr.get(tname);
            auto* row = new QLabel(QString("%1  -  %2").arg(th.name, th.desc), c.body);
            row->setAutoFillBackground(true);
            row->setCursor(Qt::PointingHandCursor);
            row->setMargin(0);
            row->setIndent(14);
            row->setWordWrap(false);
            row->setFixedHeight(44);
            row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            row->setProperty(kThemeNameProp, tname);
            row->setProperty(kRoleProp, "theme_row");
            row->setProperty(kHoveredProp, false);
            row->installEventFilter(this);
            bl->addWidget(row);
            c.theme_rows.push_back(row);
        }
        c.body->hide();
        body_layout_->addWidget(c.body);

        categories_.push_back(c);
    }
    body_layout_->addStretch(1);
}

void ThemePickerDialog::popupBelow(QWidget* anchor) {
    original_theme_name_ = themes::ThemeManager::instance().currentName();
    pending_theme_name_ = original_theme_name_;
    updateButtonState();
    refresh_rows();
    if (anchor) {
        QWidget* host = anchor->window();
        if (host) {
            const QRect host_geom = host->frameGeometry();
            move(host_geom.center() - rect().center());
        } else {
            move(anchor->mapToGlobal(QPoint(24, 24)));
        }
    } else if (parentWidget()) {
        const QRect host_geom = parentWidget()->frameGeometry();
        move(host_geom.center() - rect().center());
    } else {
        const QScreen* s = screen();
        if (s) {
            const QRect avail = s->availableGeometry();
            move(avail.center() - rect().center());
        }
    }
    show();
    raise();
    activateWindow();
}

void ThemePickerDialog::toggle_category(int idx) {
    if (idx < 0 || idx >= categories_.size()) return;
    auto& c = categories_[idx];
    c.expanded = !c.expanded;
    c.header->setText(QString("%1 %2").arg(c.expanded ? "v" : ">", c.name));
    c.body->setVisible(c.expanded);
}

void ThemePickerDialog::style_header(QLabel* h, bool hovered) {
    if (!theme_) return;
    QPalette p = h->palette();
    p.setColor(QPalette::Window,     hovered ? theme_->surface_hi : theme_->bg_toolbar);
    p.setColor(QPalette::WindowText, theme_->secondary);
    h->setPalette(p);
    // Classical section-divider treatment: a 3 px left accent bar in the
    // secondary (gold/amber/ice) colour anchors the header like a rubric drop
    // cap, with a subtle top-to-bottom gradient on the background itself.
    const QColor bg_top = hovered ? theme_->surface_hi.lighter(110)
                                  : theme_->surface.lighter(105);
    const QColor bg_bot = hovered ? theme_->surface_hi : theme_->bg_toolbar;
    h->setStyleSheet(QString(
        "QLabel {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2);"
        " color:%3;"
        " border-top:1px solid %4;"
        " border-bottom:1px solid %5;"
        " border-left:3px solid %3;"
        " padding:0px 0px 0px 10px;"
        "}")
        .arg(bg_top.name(QColor::HexRgb),          // %1 gradient top
             bg_bot.name(QColor::HexRgb),           // %2 gradient bottom
             theme_->secondary.name(QColor::HexRgb),// %3 text + left accent
             theme_->surface_hi.name(QColor::HexRgb),// %4 top edge
             theme_->rule.name(QColor::HexRgb)));    // %5 bottom rule
    h->setMinimumHeight(36);
    h->setMaximumHeight(36);
}

void ThemePickerDialog::style_theme_row(QLabel* row, bool hovered) {
    if (!theme_) return;
    const bool selected = row->property(kThemeNameProp).toString() == pending_theme_name_;
    const QColor bg = selected ? theme_->primary
                    : hovered ? theme_->surface_hi
                              : theme_->surface;
    const QColor fg = selected ? theme_->parchment : theme_->parchment;
    QPalette p = row->palette();
    p.setColor(QPalette::Window,     bg);
    p.setColor(QPalette::WindowText, fg);
    row->setPalette(p);
}

void ThemePickerDialog::update_theme_row_height(QLabel* row) {
    if (!row) return;
    const int row_height = 44;
    row->setProperty(kRowHeightProp, row_height);
    row->setFixedHeight(row_height);
}

void ThemePickerDialog::refresh_rows() {
    if (!theme_) return;
    for (auto& c : categories_) {
        style_header(c.header, false);
        for (auto* r : c.theme_rows) {
            r->setStyleSheet(QString(
                "QLabel { border-bottom:1px solid %1; padding:0px; }")
                .arg(theme_->rule_dim.name(QColor::HexRgb)));
            update_theme_row_height(r);
            style_theme_row(r, r->property(kHoveredProp).toBool());
        }
    }
    if (body_layout_) body_layout_->invalidate();
}

void ThemePickerDialog::applyTheme(const themes::Theme& t) {
    theme_ = &t;

    // Resolve fonts once — title uses font_title, list body uses font_main.
    const QString title_family = t.font_title.isEmpty() ? "Georgia"  : t.font_title;
    const QString body_family  = t.font_main.isEmpty()  ? "Segoe UI" : t.font_main;

    QPalette p = palette();
    p.setColor(QPalette::Window,     t.surface);
    p.setColor(QPalette::WindowText, t.parchment);
    setPalette(p);
    setAutoFillBackground(true);
    // Dialog buttons inherit from the dialog-level QSS but get classical bevel
    // treatment: gradient top-to-bottom with light/dark border edges.
    setStyleSheet(QString(
        "ThemePickerDialog {"
        " background:%1;"
        " border:1px solid %2;"
        "}"
        "QPushButton {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %3, stop:1 %4);"
        " color:%5;"
        " border:1px solid %6;"
        " border-bottom-color:%7;"
        " border-right-color:%7;"
        " border-radius:2px;"
        " padding:6px 14px;"
        " font-weight:600;"
        "}"
        "QPushButton:hover {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %8, stop:1 %4);"
        "}")
        .arg(t.surface.name(QColor::HexRgb),              // %1
             t.rule.name(QColor::HexRgb),                  // %2
             t.surface_hi.lighter(108).name(QColor::HexRgb),// %3 btn grad top
             t.surface.name(QColor::HexRgb),               // %4 btn grad bottom
             t.parchment.name(QColor::HexRgb),             // %5 btn text
             t.surface_hi.lighter(130).name(QColor::HexRgb),// %6 light border
             t.surface.darker(140).name(QColor::HexRgb),   // %7 dark border
             t.primary_hi.lighter(110).name(QColor::HexRgb)));// %8 hover top

    // Title bar: the theme font at a slightly larger weight, with the same
    // gradient treatment as the ShapeTreePanel header.
    QPalette tp = title_->palette();
    tp.setColor(QPalette::Window,     t.bg_toolbar);
    tp.setColor(QPalette::WindowText, t.parchment);
    title_->setPalette(tp);
    QFont title_font(title_family, 11, QFont::Bold);
    title_->setFont(title_font);
    title_->setStyleSheet(QString(
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

    // Apply the body font to every theme-row label so list text uses the
    // theme-appropriate typeface (e.g. Palatino for Arcane rows).
    for (auto& c : categories_) {
        if (c.header) {
            QFont hf(body_family);
            hf.setBold(true);
            c.header->setFont(hf);
        }
        for (auto* r : c.theme_rows) {
            if (r) r->setFont(QFont(body_family));
        }
    }

    if (body_) {
        QPalette bp = body_->palette();
        bp.setColor(QPalette::Window, t.bg_toolbar);
        body_->setPalette(bp);
        body_->setAutoFillBackground(true);
    }
    if (scroll_ && scroll_->viewport()) {
        QPalette sp = scroll_->viewport()->palette();
        sp.setColor(QPalette::Window, t.bg_toolbar);
        scroll_->viewport()->setPalette(sp);
        scroll_->viewport()->setAutoFillBackground(true);
    }
    if (ok_btn_) {
        QPalette p = ok_btn_->palette();
        p.setColor(QPalette::Button, t.primary);
        p.setColor(QPalette::ButtonText, t.parchment);
        ok_btn_->setPalette(p);
        ok_btn_->setAutoFillBackground(true);
    }
    if (cancel_btn_) {
        QPalette p = cancel_btn_->palette();
        p.setColor(QPalette::Button, t.surface_hi);
        p.setColor(QPalette::ButtonText, t.parchment);
        cancel_btn_->setPalette(p);
        cancel_btn_->setAutoFillBackground(true);
    }

    refresh_rows();
}

void ThemePickerDialog::updateButtonState() {
    if (ok_btn_) ok_btn_->setEnabled(!pending_theme_name_.isEmpty());
}

void ThemePickerDialog::reject() {
    emit themeSelectionCanceled(original_theme_name_);
    hide();
}

void ThemePickerDialog::resizeEvent(QResizeEvent* e) {
    QDialog::resizeEvent(e);
    refresh_rows();
}

bool ThemePickerDialog::eventFilter(QObject* obj, QEvent* e) {
    const QString role = obj->property(kRoleProp).toString();
    if (role == "cat_header") {
        if (e->type() == QEvent::Enter)      { style_header(qobject_cast<QLabel*>(obj), true);  return false; }
        if (e->type() == QEvent::Leave)      { style_header(qobject_cast<QLabel*>(obj), false); return false; }
        if (e->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::LeftButton) {
                toggle_category(obj->property(kCatIdxProp).toInt());
                return true;
            }
        }
    }
    if (role == "theme_row") {
        auto* row = qobject_cast<QLabel*>(obj);
        if (e->type() == QEvent::Enter) {
            row->setProperty(kHoveredProp, true);
            style_theme_row(row, true);
            return false;
        }
        if (e->type() == QEvent::Leave) {
            row->setProperty(kHoveredProp, false);
            style_theme_row(row, false);
            return false;
        }
        if (e->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::LeftButton) {
                pending_theme_name_ = obj->property(kThemeNameProp).toString();
                emit themeSelected(pending_theme_name_);
                updateButtonState();
                refresh_rows();
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, e);
}

} // namespace uvc::ui
