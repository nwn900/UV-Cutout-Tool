#pragma once

#include <QDialog>

namespace uvc::themes { struct Theme; }

namespace uvc::ui {

// Scrollable "Info" credits dialog. 1:1 port of Python `show_credits`
// (lines 2735-2838 of UV Cutout Tool.py): a modeless 560x480 window with
// sections for "What is this?", "Best For", "Limitations", "Workflow",
// "Controls", and author credit link. Escape closes.
class InfoPopup : public QDialog {
    Q_OBJECT
public:
    explicit InfoPopup(QWidget* parent = nullptr);
    void applyTheme(const themes::Theme& t);

protected:
    void keyPressEvent(QKeyEvent* e) override;
};

} // namespace uvc::ui
