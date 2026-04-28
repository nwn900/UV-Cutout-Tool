#pragma once

#include "ExportDialog.h"

#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QStackedWidget>
#include <QStringList>

#include <vector>

namespace uvc::themes { struct Theme; }

namespace uvc::ui {

class UVCanvasWidget;
class WelcomeWidget;
class WarmButton;
class ShapeTreePanel;
class ThemePickerDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);

public slots:
    void loadMesh();
    void loadDiffuse();
    void openExportDialog();
    void selectAll();
    void deselectAll();
    void invertSelection();
    void toggleAlpha();
    void backToHome();
    void showWorkspace();
    void openThemeMenu();
    void applyTheme(const QString& name);
    void undo();
    void redo();
    void openSettingsMenuFromToolbar();

private slots:
    void onMeshSelectionToggleRequested(int mesh_idx);
    void onIslandSelectionToggleRequested(int mesh_idx, int island_idx);
    void onMeshVisibilityToggleRequested(int mesh_idx);
    void onCanvasSelectionChanged();

private:
    void buildToolbar();
    void buildStatusBar();
    void buildShapeTreeDock();
    void applyCurrentTheme();
    void applyThemeVisuals(const themes::Theme& t);
    void applySettingsMenuTheme(const themes::Theme& t);
    void openSettingsMenu(QWidget* anchor);
    void showInfoPopup();
    void updateWelcomeState();
    void updateWorkspaceChrome();
    void openThemeMenu(QWidget* anchor);
    bool loadMeshFromPath(const QString& path);
    bool loadDiffuseFromPath(const QString& path);
    QImage buildExportImage(ExportColorMode mode, bool alpha);
    void doExport(const QString& fmt, ExportColorMode mode, bool alpha,
                  int png_quality, bool tga_rle);
    void resetWorkspaceStatus();
    void updateUndoRedoButtons();
    QString loadStartupMode() const;
    void saveStartupMode(const QString& mode) const;

    void saveSelectionSnapshot();
    void restoreSelectionSnapshot(const std::vector<std::vector<bool>>& state);
    std::vector<std::vector<bool>> captureSelectionSnapshot() const;

    void resizeEvent(QResizeEvent* e) override;
    void updateToolbarForSize();

    QStackedWidget* stack_ = nullptr;
    WelcomeWidget*  welcome_ = nullptr;
    UVCanvasWidget* canvas_  = nullptr;
    QWidget*        toolbar_ = nullptr;
    QDockWidget*    shape_dock_ = nullptr;
    ShapeTreePanel* shape_tree_ = nullptr;
    ThemePickerDialog* theme_dialog_ = nullptr;
    ExportDialog*   export_dialog_ = nullptr;
    QPointer<QMenu> settings_menu_;
    QPointer<QWidget> last_settings_anchor_;
    qint64 last_settings_menu_close_ms_ = 0;
    WarmButton*     btn_home_ = nullptr;
    WarmButton*     btn_mesh_ = nullptr;
    WarmButton*     btn_diff_ = nullptr;
    WarmButton*     btn_all_  = nullptr;
    WarmButton*     btn_none_ = nullptr;
    WarmButton*     btn_inv_  = nullptr;
    WarmButton*     btn_export_ = nullptr;
    WarmButton*     btn_undo_ = nullptr;
    WarmButton*     btn_redo_ = nullptr;
    WarmButton*     settings_btn_ = nullptr;
    WarmButton*     alpha_btn_ = nullptr;
    QLabel*         status_lbl_ = nullptr;
    QLabel*         hover_lbl_  = nullptr;
    QString         startup_mode_ = "welcome";

    bool alpha_on_ = true;
    QStringList mesh_paths_;
    QString diffuse_path_;

    std::vector<std::vector<std::vector<bool>>> undo_stack_;
    std::vector<std::vector<std::vector<bool>>> redo_stack_;
    static constexpr int kMaxUndoStates = 50;
};

} // namespace uvc::ui
