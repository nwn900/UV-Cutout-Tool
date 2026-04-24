#include "WelcomeWidget.h"
#include "WarmButton.h"
#include "../themes/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace uvc::ui {

WelcomeWidget::WelcomeWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* top_row = new QHBoxLayout();
    top_row->setContentsMargins(0, 0, 0, 0);
    top_row->addStretch();
    settings_btn_ = new WarmButton(QString::fromUtf8("\xE2\x9A\x99"), WarmButton::Secondary, this);
    QFont gearf; gearf.setPointSize(14);
    settings_btn_->setFont(gearf);
    settings_btn_->setFixedSize(38, 34);
    top_row->addWidget(settings_btn_, 0, Qt::AlignRight);
    outer->addLayout(top_row);
    outer->addSpacing(8);

    auto* inner_container = new QWidget(this);
    auto* inner = new QVBoxLayout(inner_container);
    inner->setAlignment(Qt::AlignCenter);

    title_ = new QLabel("UV Cutout Tool", inner_container);
    title_->setAlignment(Qt::AlignCenter);
    QFont tf; tf.setPointSize(28); tf.setBold(true); tf.setFamily("Segoe UI");
    title_->setFont(tf);
    inner->addWidget(title_);

    subtitle_ = new QLabel("Create Texture Cutouts from Mesh UV Layouts", inner_container);
    subtitle_->setAlignment(Qt::AlignCenter);
    QFont sf; sf.setPointSize(11); sf.setItalic(true); sf.setFamily("Segoe UI");
    subtitle_->setFont(sf);
    inner->addWidget(subtitle_);

    inner->addSpacing(24);
    auto* rule = new QFrame(inner_container);
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedWidth(320);
    rule->setFixedHeight(1);
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
    QPalette p = palette();
    p.setColor(QPalette::Window,     t.bg_canvas);
    p.setColor(QPalette::WindowText, t.parchment);
    setPalette(p);

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
        rp.setColor(QPalette::Window, t.rule_dim);
        footer_rule_->setPalette(rp);
    }

    settings_btn_->applyTheme(t);
    load_mesh_->applyTheme(t);
    load_tex_ ->applyTheme(t);
    open_ws_  ->applyTheme(t);
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

} // namespace uvc::ui
