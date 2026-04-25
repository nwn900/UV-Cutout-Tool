#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QStackedWidget;

namespace uvc::themes { struct Theme; }

namespace uvc::ui {

enum class ExportColorMode { FullColor, Grayscale, BlackWhite };

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportDialog(QWidget* parent = nullptr);

    void applyTheme(const themes::Theme& t);

    // Provide the base stem of the loaded diffuse (e.g. "body_d") so the
    // filename preview can show "body_d_cutout.png" and update live.
    void setDiffuseStem(const QString& stem);

    QString         format()      const;   // "png" or "tga"
    ExportColorMode colorMode()   const;
    bool            includeAlpha() const;
    int             pngQuality()  const;   // 0–100 for QImage::save / QImageWriter
    bool            tgaRle()      const;

private:
    void updateFormatPanel();
    void updateFilenamePreview();
    void applyComboTheme(const themes::Theme& t);

    QLabel*        title_       = nullptr;
    QComboBox*     fmt_combo_   = nullptr;
    QComboBox*     mode_combo_  = nullptr;
    QCheckBox*     alpha_check_ = nullptr;
    QStackedWidget* fmt_stack_  = nullptr;

    // PNG page
    QComboBox*  png_quality_combo_ = nullptr;
    // TGA page
    QCheckBox*  tga_rle_check_     = nullptr;

    QLabel*  filename_lbl_  = nullptr;
    QLabel*  hint_lbl_      = nullptr;

    QString diffuse_stem_;
};

} // namespace uvc::ui
