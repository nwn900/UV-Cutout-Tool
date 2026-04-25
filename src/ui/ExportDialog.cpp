#include "ExportDialog.h"

#include "../themes/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace uvc::ui {

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle("Export Cutout");
    setFixedSize(420, 330);
    setSizeGripEnabled(false);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Title bar ──────────────────────────────────────────────────────────
    title_ = new QLabel("Export Cutout", this);
    title_->setAlignment(Qt::AlignCenter);
    title_->setAutoFillBackground(true);
    title_->setMargin(8);
    {
        QFont f = title_->font();
        f.setBold(true);
        f.setPointSize(10);
        title_->setFont(f);
    }
    outer->addWidget(title_);

    // ── Content ────────────────────────────────────────────────────────────
    auto* content = new QWidget(this);
    content->setAutoFillBackground(true);
    auto* grid = new QGridLayout(content);
    grid->setContentsMargins(22, 16, 22, 10);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);

    // Format row
    auto* fmt_lbl = new QLabel("Format:", content);
    fmt_combo_ = new QComboBox(content);
    fmt_combo_->addItem("PNG",  "png");
    fmt_combo_->addItem("TGA",  "tga");
    grid->addWidget(fmt_lbl,   0, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(fmt_combo_,0, 1);

    // Color mode row
    auto* mode_lbl = new QLabel("Color mode:", content);
    mode_combo_ = new QComboBox(content);
    mode_combo_->addItem("Full Color",               int(ExportColorMode::FullColor));
    mode_combo_->addItem("Grayscale",                int(ExportColorMode::Grayscale));
    mode_combo_->addItem("Black & White (mask)",     int(ExportColorMode::BlackWhite));
    grid->addWidget(mode_lbl,   1, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(mode_combo_,1, 1);

    // Alpha checkbox row
    alpha_check_ = new QCheckBox("Include alpha channel", content);
    alpha_check_->setChecked(true);
    grid->addWidget(alpha_check_, 2, 1);

    // Hint — sits directly under the alpha checkbox, always visible.
    hint_lbl_ = new QLabel(content);
    hint_lbl_->setObjectName("hint_lbl");
    hint_lbl_->setWordWrap(true);
    {
        QFont f = hint_lbl_->font();
        f.setPointSize(f.pointSize() - 1);
        hint_lbl_->setFont(f);
    }
    grid->addWidget(hint_lbl_, 3, 1);

    // ── Format-specific settings (stacked) ────────────────────────────────
    // Thin separator line
    auto* sep = new QFrame(content);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    grid->addWidget(sep, 4, 0, 1, 2);

    fmt_stack_ = new QStackedWidget(content);

    // -- PNG page --
    auto* png_page = new QWidget(fmt_stack_);
    auto* png_lay  = new QGridLayout(png_page);
    png_lay->setContentsMargins(0, 0, 0, 0);
    png_lay->setHorizontalSpacing(12);
    png_lay->setVerticalSpacing(6);
    png_lay->setColumnStretch(1, 1);
    auto* png_lbl = new QLabel("Compression:", png_page);
    png_quality_combo_ = new QComboBox(png_page);
    // Qt 6 PNG quality formula: zlib_level = (100 - quality) * 9 / 91
    // So quality=100 → zlib 0 (no compression), quality=0 → zlib 9 (max).
    png_quality_combo_->addItem("None  –  fastest, largest file",  100);
    png_quality_combo_->addItem("Low",                              75);
    png_quality_combo_->addItem("Medium  –  balanced",              50);
    png_quality_combo_->addItem("High  –  slowest, smallest file",   0);
    png_quality_combo_->setCurrentIndex(0); // None default
    png_lay->addWidget(png_lbl,           0, 0, Qt::AlignRight | Qt::AlignVCenter);
    png_lay->addWidget(png_quality_combo_,0, 1);
    fmt_stack_->addWidget(png_page);

    // -- TGA page --
    auto* tga_page = new QWidget(fmt_stack_);
    auto* tga_lay  = new QGridLayout(tga_page);
    tga_lay->setContentsMargins(0, 0, 0, 0);
    tga_lay->setColumnStretch(1, 1);
    tga_rle_check_ = new QCheckBox("RLE compression  –  smaller file, slightly slower", tga_page);
    tga_lay->addWidget(new QLabel(QString(), tga_page), 0, 0); // spacer to match column alignment
    tga_lay->addWidget(tga_rle_check_, 0, 1);
    fmt_stack_->addWidget(tga_page);

    grid->addWidget(fmt_stack_, 5, 0, 1, 2);

    // Separator before the filename row
    auto* sep2 = new QFrame(content);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Plain);
    grid->addWidget(sep2, 6, 0, 1, 2);

    // Filename preview
    auto* fn_key = new QLabel("Filename:", content);
    filename_lbl_ = new QLabel("—", content);
    filename_lbl_->setObjectName("filename_lbl");
    {
        QFont f = filename_lbl_->font();
        f.setItalic(true);
        filename_lbl_->setFont(f);
    }
    grid->addWidget(fn_key,        7, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(filename_lbl_, 7, 1);

    outer->addWidget(content, 1);

    // ── Buttons ────────────────────────────────────────────────────────────
    auto* btn_area = new QWidget(this);
    auto* btn_lay  = new QHBoxLayout(btn_area);
    btn_lay->setContentsMargins(22, 0, 22, 14);
    btn_lay->setSpacing(10);
    btn_lay->addStretch(1);
    auto* cancel_btn = new QPushButton("Cancel", btn_area);
    auto* export_btn = new QPushButton("Export…", btn_area);
    export_btn->setObjectName("export_btn");
    export_btn->setDefault(true);
    for (auto* b : {cancel_btn, export_btn}) {
        b->setFixedHeight(30);
        b->setMinimumWidth(88);
    }
    btn_lay->addWidget(cancel_btn);
    btn_lay->addWidget(export_btn);
    outer->addWidget(btn_area);

    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(export_btn, &QPushButton::clicked, this, &QDialog::accept);

    connect(fmt_combo_,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateFormatPanel(); updateFilenamePreview(); });

    auto updateHint = [this](bool checked) {
        if (!hint_lbl_) return;
        hint_lbl_->setText(checked
            ? "Selected areas are fully opaque. Everything outside the cutout is transparent."
            : "Selected areas are fully opaque. Everything outside the cutout is solid black.");
    };
    connect(alpha_check_, &QCheckBox::toggled, this, updateHint);
    updateHint(alpha_check_->isChecked());

    updateFormatPanel();
    updateFilenamePreview();
}

void ExportDialog::updateFormatPanel() {
    if (!fmt_stack_) return;
    fmt_stack_->setCurrentIndex(fmt_combo_->currentData().toString() == "tga" ? 1 : 0);
}

void ExportDialog::updateFilenamePreview() {
    if (!filename_lbl_) return;
    const QString ext = fmt_combo_->currentData().toString();
    const QString stem = diffuse_stem_.isEmpty() ? "Diffuse_Cutouts" : diffuse_stem_ + "_cutout";
    filename_lbl_->setText(QString("%1.%2").arg(stem, ext));
}

void ExportDialog::setDiffuseStem(const QString& stem) {
    diffuse_stem_ = stem;
    updateFilenamePreview();
}

QString ExportDialog::format() const {
    return fmt_combo_->currentData().toString();
}

ExportColorMode ExportDialog::colorMode() const {
    return static_cast<ExportColorMode>(mode_combo_->currentData().toInt());
}

bool ExportDialog::includeAlpha() const {
    return alpha_check_->isChecked();
}

int ExportDialog::pngQuality() const {
    return png_quality_combo_->currentData().toInt();
}

bool ExportDialog::tgaRle() const {
    return tga_rle_check_->isChecked();
}

void ExportDialog::applyTheme(const themes::Theme& t) {
    QPalette p = palette();
    p.setColor(QPalette::Window,     t.surface);
    p.setColor(QPalette::WindowText, t.parchment);
    setPalette(p);
    setAutoFillBackground(true);

    if (title_) {
        title_->setStyleSheet(QString(
            "QLabel {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1,stop:1 %2);"
            " color:%3;"
            " border-bottom:1px solid %4;"
            " padding:8px 10px;"
            "}")
            .arg(t.surface.name(QColor::HexRgb),
                 t.bg_toolbar.name(QColor::HexRgb),
                 t.parchment.name(QColor::HexRgb),
                 t.rule.name(QColor::HexRgb)));
    }

    const QString dim_css =
        QString("QLabel { color:%1; background:transparent; }").arg(t.parchment.name(QColor::HexRgb));
    if (hint_lbl_)     hint_lbl_->setStyleSheet(dim_css);
    if (filename_lbl_) filename_lbl_->setStyleSheet(dim_css);

    // Full stylesheet covering the content area, combos, checkbox, buttons.
    setStyleSheet(QString(
        "ExportDialog {"
        " background:%1;"
        "}"
        "QWidget {"
        " background:%1;"
        " color:%2;"
        "}"
        "QLabel {"
        " background:transparent;"
        " color:%2;"
        "}"
        "QFrame[frameShape='4'],"   // HLine separator
        "QFrame[frameShape='5'] {"  // VLine
        " background:%3;"
        " border:none;"
        " max-height:1px;"
        "}"
        "QComboBox {"
        " background:%4;"
        " color:%2;"
        " border:1px solid %3;"
        " border-radius:3px;"
        " padding:4px 8px;"
        " min-height:24px;"
        "}"
        "QComboBox::drop-down { border:none; width:22px; }"
        "QComboBox QAbstractItemView {"
        " background:%5;"
        " color:%2;"
        " border:1px solid %3;"
        " selection-background-color:%6;"
        " selection-color:%2;"
        " outline:none;"
        "}"
        "QCheckBox { color:%2; background:transparent; spacing:6px; }"
        "QCheckBox::indicator {"
        " width:14px; height:14px;"
        " border:1px solid %3;"
        " border-radius:2px;"
        " background:%4;"
        "}"
        "QCheckBox::indicator:checked {"
        " background:%6;"
        " border-color:%6;"
        "}"
        "QPushButton {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %7,stop:1 %4);"
        " color:%2;"
        " border:1px solid %3;"
        " border-radius:3px;"
        " padding:5px 18px;"
        "}"
        "QPushButton:hover { background:%7; }"
        "QPushButton#export_btn {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %8,stop:1 %6);"
        " color:%2;"
        " border-color:%3;"
        "}"
        "QPushButton#export_btn:hover { background:%8; }")
        .arg(t.surface.name(QColor::HexRgb),           // %1 bg
             t.parchment.name(QColor::HexRgb),          // %2 text
             t.rule.name(QColor::HexRgb),               // %3 border/separator
             t.bg_toolbar.name(QColor::HexRgb),         // %4 input bg
             t.bg_panel.name(QColor::HexRgb),           // %5 dropdown popup bg
             t.primary.name(QColor::HexRgb),            // %6 primary / checked
             t.surface_hi.name(QColor::HexRgb),         // %7 cancel hover
             t.primary_hi.name(QColor::HexRgb)));       // %8 export hover
}

} // namespace uvc::ui
