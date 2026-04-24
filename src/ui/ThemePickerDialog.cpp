#include "ThemePickerDialog.h"

#include "../themes/Theme.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace uvc::ui {

namespace {
constexpr const char* kCatIdxProp   = "uvc.cat_idx";
constexpr const char* kThemeNameProp = "uvc.theme_name";
constexpr const char* kRoleProp     = "uvc.role";
constexpr const char* kHoveredProp  = "uvc.hovered";
} // namespace

ThemePickerDialog::ThemePickerDialog(QWidget* parent)
    : QDialog(parent, Qt::Dialog) {
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle("Themes");
    resize(560, 480);
    setMinimumSize(560, 480);

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
        c.header->setMargin(6);
        c.header->setFixedHeight(32);
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
            auto* row = new QLabel(QString("  %1  -  %2").arg(th.name, th.desc), c.body);
            row->setAutoFillBackground(true);
            row->setCursor(Qt::PointingHandCursor);
            row->setMargin(6);
            row->setMinimumHeight(32);
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
}

void ThemePickerDialog::style_theme_row(QLabel* row, bool hovered) {
    if (!theme_) return;
    QPalette p = row->palette();
    p.setColor(QPalette::Window,     hovered ? theme_->surface_hi : theme_->surface);
    p.setColor(QPalette::WindowText, theme_->parchment);
    row->setPalette(p);
}

void ThemePickerDialog::applyTheme(const themes::Theme& t) {
    theme_ = &t;

    QPalette p = palette();
    p.setColor(QPalette::Window,     t.surface);
    p.setColor(QPalette::WindowText, t.parchment);
    setPalette(p);
    setAutoFillBackground(true);

    QPalette tp = title_->palette();
    tp.setColor(QPalette::Window,     t.bg_toolbar);
    tp.setColor(QPalette::WindowText, t.parchment);
    title_->setPalette(tp);

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

    for (auto& c : categories_) {
        style_header(c.header, false);
        for (auto* r : c.theme_rows) style_theme_row(r, r->property(kHoveredProp).toBool());
    }
}

void ThemePickerDialog::updateButtonState() {
    if (ok_btn_) ok_btn_->setEnabled(!pending_theme_name_.isEmpty());
}

void ThemePickerDialog::reject() {
    emit themeSelectionCanceled(original_theme_name_);
    hide();
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
        if (e->type() == QEvent::Enter)      { row->setProperty(kHoveredProp, true);  style_theme_row(row, true);  return false; }
        if (e->type() == QEvent::Leave)      { row->setProperty(kHoveredProp, false); style_theme_row(row, false); return false; }
        if (e->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::LeftButton) {
                pending_theme_name_ = obj->property(kThemeNameProp).toString();
                emit themePreviewed(pending_theme_name_);
                updateButtonState();
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, e);
}

} // namespace uvc::ui
