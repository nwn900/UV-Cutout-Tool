#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QStackedWidget>

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
    void exportTGA();
    void exportPNG();
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
    void toggleFullscreen();
    void exitFullscreen();
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
    void openSettingsMenu(QWidget* anchor);
    void showInfoPopup();
    void updateWelcomeState();
    void updateWorkspaceChrome();
    void openThemeMenu(QWidget* anchor);
    bool loadMeshFromPath(const QString& path);
    bool loadDiffuseFromPath(const QString& path);
    QImage buildExportImage();
    // Shared body of exportTGA/exportPNG — matches Python `_do_export(fmt)`
    // at lines 4763-4805: validation, default filename from diffuse stem,
    // save dialog anchored in exe dir, rasterize, write, status w/ path.
    void doExport(const QString& fmt);
    // Set the left-side status label to the workspace intro hint
    // ("{W}×{H} UV space  ·  Click/drag to select  ·  ..."). Mirrors Python
    // `_reset_workspace_status`.
    void resetWorkspaceStatus();
    void updateUndoRedoButtons();
    QString loadStartupMode() const;
    void saveStartupMode(const QString& mode) const;

    // Capture the current selection bits into the undo stack and clear redo.
    // Called via the canvas's `selectionAboutToChange` signal and directly
    // from shape-tree interactions.
    void saveSelectionSnapshot();
    // Apply a snapshot to `canvas_`'s mesh vector and rebuild derived state.
    void restoreSelectionSnapshot(const std::vector<std::vector<bool>>& state);
    std::vector<std::vector<bool>> captureSelectionSnapshot() const;

    QStackedWidget* stack_ = nullptr;
    WelcomeWidget*  welcome_ = nullptr;
    UVCanvasWidget* canvas_  = nullptr;
    QWidget*        toolbar_ = nullptr;
    QDockWidget*    shape_dock_ = nullptr;
    ShapeTreePanel* shape_tree_ = nullptr;
    ThemePickerDialog* theme_dialog_ = nullptr;
    QPointer<QMenu> settings_menu_;
    QPointer<QWidget> last_settings_anchor_;
    qint64 last_settings_menu_close_ms_ = 0;
    WarmButton*     btn_home_ = nullptr;
    WarmButton*     btn_mesh_ = nullptr;
    WarmButton*     btn_diff_ = nullptr;
    WarmButton*     btn_all_  = nullptr;
    WarmButton*     btn_none_ = nullptr;
    WarmButton*     btn_inv_  = nullptr;
    WarmButton*     btn_tga_  = nullptr;
    WarmButton*     btn_png_  = nullptr;
    WarmButton*     btn_undo_ = nullptr;
    WarmButton*     btn_redo_ = nullptr;
    WarmButton*     settings_btn_ = nullptr;
    QLabel*         alpha_btn_ = nullptr;
    QLabel*         status_lbl_ = nullptr;
    QLabel*         hover_lbl_  = nullptr;
    QString         startup_mode_ = "welcome";

    bool alpha_on_ = true;
    QString mesh_path_;
    // Source path of the currently-loaded diffuse, if any. Used to derive the
    // default export filename ("{stem}_cutout.{fmt}") in doExport — matches
    // Python `self.diffuse_path` (lines 4779-4783).
    QString diffuse_path_;

    std::vector<std::vector<std::vector<bool>>> undo_stack_;
    std::vector<std::vector<std::vector<bool>>> redo_stack_;
    static constexpr int kMaxUndoStates = 50;
};

} // namespace uvc::ui
