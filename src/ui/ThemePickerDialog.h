#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QVector>

namespace uvc::themes { struct Theme; }

class QResizeEvent;

namespace uvc::ui {

// Frameless theme popup anchored below the theme button on the welcome screen.
// Categories are collapsible; clicking a theme row applies it immediately.
class ThemePickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemePickerDialog(QWidget* parent = nullptr);

    void applyTheme(const themes::Theme& t);

    // Anchor the popup directly below the given widget.
    void popupBelow(QWidget* anchor);

signals:
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
    void update_theme_row_height(QLabel* row);
    void refresh_rows();
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
    void resizeEvent(QResizeEvent* e) override;
    void reject() override;
};

} // namespace uvc::ui
