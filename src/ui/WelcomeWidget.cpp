#include "WelcomeWidget.h"
#include "WarmButton.h"
#include "../themes/Theme.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLinearGradient>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QVBoxLayout>

namespace uvc::ui {

namespace {

QIcon make_gear_icon(const QColor& color) {
    QPixmap pm(24, 24);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(12, 12);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    for (int i = 0; i < 8; ++i) {
        p.save();
        p.rotate(i * 45.0);
        p.drawRoundedRect(QRectF(-2.0, -11.0, 4.0, 5.0), 1.0, 1.0);
        p.restore();
    }
    p.drawEllipse(QPointF(0, 0), 7.0, 7.0);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.drawEllipse(QPointF(0, 0), 3.0, 3.0);
    return QIcon(pm);
}

} // namespace

WelcomeWidget::WelcomeWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("welcomeRoot");
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* top_row = new QHBoxLayout();
    top_row->setContentsMargins(0, 0, 0, 0);
    top_row->addStretch();
    settings_btn_ = new WarmButton(QString(), WarmButton::Secondary, this);
    settings_btn_->setToolTip("Settings");
    settings_btn_->setAccessibleName("Settings");
    settings_btn_->setIconSize(QSize(18, 18));
    settings_btn_->setFixedSize(38, 34);
    top_row->addWidget(settings_btn_, 0, Qt::AlignRight);
    outer->addLayout(top_row);
    outer->addSpacing(8);

    auto* inner_container = new QWidget(this);
    inner_container->setObjectName("welcomePanel");
    inner_container->setAttribute(Qt::WA_StyledBackground, true);
    inner_container->setAutoFillBackground(true);
    auto* inner = new QVBoxLayout(inner_container);
    inner->setAlignment(Qt::AlignCenter);
    inner->setContentsMargins(38, 32, 38, 30);

    title_ = new QLabel("UV Cutout Tool", inner_container);
    title_->setAlignment(Qt::AlignCenter);
    QFont tf; tf.setPointSize(30); tf.setBold(true); tf.setFamily("Georgia");
    title_->setFont(tf);
    inner->addWidget(title_);

    subtitle_ = new QLabel("Create Texture Cutouts from Mesh UV Layouts", inner_container);
    subtitle_->setAlignment(Qt::AlignCenter);
    QFont sf; sf.setPointSize(11); sf.setItalic(true); sf.setFamily("Georgia");
    subtitle_->setFont(sf);
    inner->addWidget(subtitle_);

    inner->addSpacing(24);
    auto* rule = new QFrame(inner_container);
    rule->setObjectName("welcomeRule");
    rule->setFrameShape(QFrame::NoFrame);
    rule->setFixedWidth(320);
    rule->setFixedHeight(3);
    inner->addWidget(rule, 0, Qt::AlignCenter);
    inner->addSpacing(18);

    load_mesh_ = new WarmButton("  Load NIF Mesh  ", WarmButton::Welcome, inner_container);
    QFont bf; bf.setPointSize(13); bf.setBold(true);
    load_mesh_->setFont(bf);
    load_mesh_->setFixedWidth(320);
    inner->addWidget(load_mesh_, 0, Qt::AlignCenter);
    inner->addSpacing(12);

    load_tex_ = new WarmButton("  Load Diffuse Texture  ", WarmButton::Secondary, inner_container);
    QFont mf; mf.setPointSize(10);
    load_tex_->setFont(mf);
    load_tex_->setFixedWidth(320);
    inner->addWidget(load_tex_, 0, Qt::AlignCenter);
    inner->addSpacing(16);

    QFont df; df.setItalic(true); df.setPointSize(8);

    mesh_file_lbl_ = new QLabel("Mesh: none loaded", inner_container);
    mesh_file_lbl_->setAlignment(Qt::AlignCenter);
    mesh_file_lbl_->setFont(df);
    inner->addWidget(mesh_file_lbl_);

    diffuse_file_lbl_ = new QLabel("Texture: none loaded", inner_container);
    diffuse_file_lbl_->setAlignment(Qt::AlignCenter);
    diffuse_file_lbl_->setFont(df);
    inner->addSpacing(2);
    inner->addWidget(diffuse_file_lbl_);

    open_ws_ = new WarmButton("  Open in Workspace  ", WarmButton::Welcome, inner_container);
    open_ws_->setFont(bf);
    open_ws_->setFixedWidth(320);
    inner->addWidget(open_ws_, 0, Qt::AlignCenter);

    inner->addSpacing(24);

    // Bottom ornamentation: thin rule and supports strip.
    footer_rule_ = new QLabel(inner_container);
    footer_rule_->setFixedSize(320, 1);
    footer_rule_->setAutoFillBackground(true);
    inner->addWidget(footer_rule_, 0, Qt::AlignCenter);
    inner->addSpacing(14);

    supports_strip_ = new QLabel(
        "Supports NIF \u00B7 PNG \u00B7 TGA \u00B7 DDS \u00B7 JPG \u00B7 BMP  |  F11 Fullscreen",
        inner_container);
    supports_strip_->setAlignment(Qt::AlignCenter);
    QFont fs; fs.setPointSize(8);
    supports_strip_->setFont(fs);
    inner->addWidget(supports_strip_);

    outer->addWidget(inner_container, 1, Qt::AlignCenter);

    connect(load_mesh_, &WarmButton::clicked, this, &WelcomeWidget::loadMeshRequested);
    connect(load_tex_,  &WarmButton::clicked, this, &WelcomeWidget::loadDiffuseRequested);
    connect(open_ws_,   &WarmButton::clicked, this, &WelcomeWidget::openWorkspaceRequested);
    connect(settings_btn_, &WarmButton::clicked, this, &WelcomeWidget::settingsRequested);
}

void WelcomeWidget::applyTheme(const themes::Theme& t) {
    bg_deep_ = t.bg_deep;
    bg_canvas_ = t.bg_canvas;
    bg_mid_ = t.bg_mid;

    QPalette p = palette();
    p.setColor(QPalette::Window,     t.bg_canvas);
    p.setColor(QPalette::WindowText, t.parchment);
    setPalette(p);
    setAutoFillBackground(true);
    setStyleSheet(QString(
        "QWidget#welcomePanel {"
        " background:%1;"
        " border:1px solid %2;"
        " border-top-color:%3;"
        "}"
        "QFrame#welcomeRule {"
        " background:qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 transparent, stop:0.18 %2, stop:0.5 %4, stop:0.82 %2, stop:1 transparent);"
        " border:none;"
        "}")
        .arg(QColor(t.bg_panel.red(), t.bg_panel.green(), t.bg_panel.blue(), 220).name(QColor::HexArgb),
             t.rule.name(QColor::HexRgb),
             t.surface_hi.name(QColor::HexRgb),
             t.secondary.name(QColor::HexRgb)));

    for (auto* lbl : {title_, subtitle_, mesh_file_lbl_, diffuse_file_lbl_, supports_strip_}) {
        if (!lbl) continue;
        QPalette lp = lbl->palette();
        lp.setColor(QPalette::Window,     t.bg_canvas);
        QColor fg = (lbl == title_)      ? t.parchment
                   : (lbl == subtitle_)  ? t.parchment_dim
                                         : t.parchment_faint;
        lp.setColor(QPalette::WindowText, fg);
        lbl->setPalette(lp);
    }

    if (footer_rule_) {
        QPalette rp = footer_rule_->palette();
        rp.setColor(QPalette::Window, t.rule);
        footer_rule_->setPalette(rp);
    }

    settings_btn_->applyTheme(t);
    settings_btn_->setIcon(make_gear_icon(t.parchment_dim));
    load_mesh_->applyTheme(t);
    load_tex_ ->applyTheme(t);
    open_ws_  ->applyTheme(t);

    for (auto* w : findChildren<QWidget*>()) {
        if (!w) continue;
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
    style()->unpolish(this);
    style()->polish(this);
    updateGeometry();
    update();
}

void WelcomeWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    QColor bg = bg_canvas_.isValid() ? bg_canvas_ : palette().color(QPalette::Window);
    p.fillRect(rect(), bg);
}

void WelcomeWidget::setLoadedFiles(const QString& mesh_file, const QString& diffuse_file) {
    if (mesh_file_lbl_)
        mesh_file_lbl_->setText(QString("Mesh: %1").arg(mesh_file.isEmpty() ? "none loaded" : mesh_file));
    if (diffuse_file_lbl_)
        diffuse_file_lbl_->setText(QString("Texture: %1").arg(diffuse_file.isEmpty() ? "none loaded" : diffuse_file));
}
void WelcomeWidget::setWorkspaceButtonHasLoadedContent(bool has_loaded_content) {
    if (!open_ws_) return;
    open_ws_->setText(has_loaded_content ? "  Open in Workspace  "
                                         : "  Enter Workspace  ");
}

bool WelcomeWidget::is_mesh_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "nif";
}

bool WelcomeWidget::is_diffuse_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "png" || ext == "tga" || ext == "dds"
        || ext == "jpg" || ext == "jpeg" || ext == "bmp";
}

QString WelcomeWidget::dropped_local_file(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) return {};
    const auto urls = mime->urls();
    if (urls.isEmpty()) return {};
    QStringList paths;
    for (const auto& url : urls) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    return paths.join("|||");
}

void WelcomeWidget::dragEnterEvent(QDragEnterEvent* e) {
    const QString paths = dropped_local_file(e->mimeData());
    if (paths.isEmpty()) {
        e->ignore();
        return;
    }
    const auto parts = paths.split("|||");
    bool has_mesh = false, has_diffuse = false;
    for (const QString& p : parts) {
        if (is_mesh_file(p)) has_mesh = true;
        else if (is_diffuse_file(p)) has_diffuse = true;
    }
    if (has_mesh || has_diffuse) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void WelcomeWidget::dragMoveEvent(QDragMoveEvent* e) {
    const QString paths = dropped_local_file(e->mimeData());
    if (paths.isEmpty()) {
        e->ignore();
        return;
    }
    const auto parts = paths.split("|||");
    bool has_mesh = false, has_diffuse = false;
    for (const QString& p : parts) {
        if (is_mesh_file(p)) has_mesh = true;
        else if (is_diffuse_file(p)) has_diffuse = true;
    }
    if (has_mesh || has_diffuse) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

void WelcomeWidget::dropEvent(QDropEvent* e) {
    const QString paths = dropped_local_file(e->mimeData());
    if (paths.isEmpty()) {
        e->ignore();
        return;
    }
    const auto parts = paths.split("|||");
    bool mesh_emitted = false, diffuse_emitted = false;
    for (const QString& p : parts) {
        if (is_mesh_file(p) && !mesh_emitted) {
            emit meshFileDropped(p);
            mesh_emitted = true;
        }
        else if (is_diffuse_file(p) && !diffuse_emitted) {
            emit diffuseFileDropped(p);
            diffuse_emitted = true;
        }
    }
    if (mesh_emitted || diffuse_emitted) {
        e->acceptProposedAction();
        return;
    }
    e->ignore();
}

} // namespace uvc::ui
