#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QVector>

namespace uvc::themes { struct Theme; }

namespace uvc::ui {

// 1:1 port of Python `_show_theme_window`: a frameless popup anchored below the
// "Theme: …" button on the welcome screen. Categories (THEME_ORDER) are
// collapsible; clicking a theme row applies it immediately.
class ThemePickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemePickerDialog(QWidget* parent = nullptr);

    void applyTheme(const themes::Theme& t);

    // Anchor the popup directly below the given widget (mirrors
    // `self._theme_popup.geometry(f"+{x}+{y}")` where y = btn_y + btn_h).
    void popupBelow(QWidget* anchor);

signals:
    void themePreviewed(const QString& name);
    void themeSelected(const QString& name);
    void themeSelectionCanceled(const QString& revert_name);

private:
    struct Category {
        QString      name;
        QLabel*      header        = nullptr;
        QWidget*     body          = nullptr;
        QVector<QLabel*> theme_rows;
        bool         expanded      = false;
    };

    void build_rows();
    void toggle_category(int idx);
    void style_header(QLabel* h, bool hovered);
    void style_theme_row(QLabel* row, bool hovered);
    void updateButtonState();

    QVBoxLayout*  body_layout_ = nullptr;
    QWidget*      body_        = nullptr;
    QScrollArea*  scroll_      = nullptr;
    QLabel*       title_       = nullptr;
    QPushButton*  ok_btn_      = nullptr;
    QPushButton*  cancel_btn_  = nullptr;
    QVector<Category> categories_;
    const themes::Theme* theme_ = nullptr;
    QString original_theme_name_;
    QString pending_theme_name_;

    bool eventFilter(QObject* obj, QEvent* e) override;
    void reject() override;
};

} // namespace uvc::ui
