#pragma once

#include <QColor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QWidget>
#include <QLabel>

namespace uvc::themes { struct Theme; }

class QPaintEvent;

namespace uvc::ui {

class WarmButton;

class WelcomeWidget : public QWidget {
    Q_OBJECT
public:
    explicit WelcomeWidget(QWidget* parent = nullptr);

    void applyTheme(const themes::Theme& t);
    void setLoadedFiles(const QString& mesh_file, const QString& diffuse_file);
    void setWorkspaceButtonHasLoadedContent(bool has_loaded_content);
    WarmButton* settingsButton() const { return settings_btn_; }

    WarmButton* meshButton() const { return load_mesh_; }
    WarmButton* diffuseButton() const { return load_tex_; }

    void acceptDrops() { setAcceptDrops(true); }

signals:
    void loadMeshRequested();
    void loadDiffuseRequested();
    void openWorkspaceRequested();
    void settingsRequested();
    void meshFileDropped(const QString& path);
    void diffuseFileDropped(const QString& path);

protected:
    void paintEvent(QPaintEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    static bool is_mesh_file(const QString& path);
    static bool is_diffuse_file(const QString& path);
    static QString dropped_local_file(const QMimeData* mime);

    QColor bg_deep_;
    QColor bg_canvas_;
    QColor bg_mid_;

    QLabel*     title_    = nullptr;
    QLabel*     subtitle_ = nullptr;
    WarmButton* settings_btn_ = nullptr;
    QLabel*     mesh_file_lbl_ = nullptr;
    QLabel*     diffuse_file_lbl_ = nullptr;
    QLabel*     footer_rule_ = nullptr;  // thin horizontal rule above supports strip
    QLabel*     supports_strip_ = nullptr;
    WarmButton* load_mesh_= nullptr;
    WarmButton* load_tex_ = nullptr;
    WarmButton* open_ws_  = nullptr;
};

} // namespace uvc::ui
