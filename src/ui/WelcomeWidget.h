#pragma once

#include <QWidget>
#include <QLabel>

namespace uvc::themes { struct Theme; }

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

signals:
    void loadMeshRequested();
    void loadDiffuseRequested();
    void openWorkspaceRequested();
    void settingsRequested();

private:
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
