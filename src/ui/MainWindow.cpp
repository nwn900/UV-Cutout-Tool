#include "MainWindow.h"
#include "InfoPopup.h"
#include "ShapeTreePanel.h"
#include "ThemePickerDialog.h"
#include "UVCanvasWidget.h"
#include "WarmButton.h"
#include "WelcomeWidget.h"

#include "../app/AppSettings.h"
#include "../codec/DDSLoader.h"
#include "../codec/TGAIO.h"
#include "../parsers/NiflyParser.h"
#include "../parsers/OBJParser.h"
#include "../themes/Theme.h"

#include <QApplication>
#include <QActionGroup>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace uvc::ui {

using themes::ThemeManager;

namespace {

bool is_mesh_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "nif" || ext == "obj";
}

bool is_diffuse_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "png" || ext == "tga" || ext == "dds"
        || ext == "jpg" || ext == "jpeg" || ext == "bmp";
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("UV Cutout Tool");
    setMinimumSize(960, 680);
    setWindowIcon(QIcon(":/icon.png"));

    welcome_ = new WelcomeWidget(this);
    canvas_  = new UVCanvasWidget(this);
    stack_   = new QStackedWidget(this);
    stack_->addWidget(welcome_);
    stack_->addWidget(canvas_);

    buildToolbar();
    buildStatusBar();
    buildShapeTreeDock();

    connect(welcome_, &WelcomeWidget::loadMeshRequested,    this, &MainWindow::loadMesh);
    connect(welcome_, &WelcomeWidget::loadDiffuseRequested, this, &MainWindow::loadDiffuse);
    connect(welcome_, &WelcomeWidget::openWorkspaceRequested, this, &MainWindow::showWorkspace);
    connect(welcome_, &WelcomeWidget::settingsRequested, this, [this] {
        if (welcome_ && welcome_->settingsButton()) openSettingsMenu(welcome_->settingsButton());
    });
    if (welcome_->meshButton()) {
        welcome_->meshButton()->setDropKind(WarmButton::MeshDrop);
        connect(welcome_->meshButton(), &WarmButton::fileDropped, this, &MainWindow::loadMeshFromPath);
    }
    if (welcome_->diffuseButton()) {
        welcome_->diffuseButton()->setDropKind(WarmButton::DiffuseDrop);
        connect(welcome_->diffuseButton(), &WarmButton::fileDropped, this, &MainWindow::loadDiffuseFromPath);
    }
    connect(canvas_, &UVCanvasWidget::hoverInfoChanged, hover_lbl_, &QLabel::setText);
    connect(canvas_, &UVCanvasWidget::selectionChanged, this, &MainWindow::onCanvasSelectionChanged);
    connect(canvas_, &UVCanvasWidget::selectionAboutToChange, this, &MainWindow::saveSelectionSnapshot);
    connect(canvas_, &UVCanvasWidget::meshFileDropped, this, &MainWindow::loadMeshFromPath);
    connect(canvas_, &UVCanvasWidget::diffuseFileDropped, this, &MainWindow::loadDiffuseFromPath);

    auto* undo_sc = new QShortcut(QKeySequence::Undo, this);
    connect(undo_sc, &QShortcut::activated, this, &MainWindow::undo);
    auto* redo_sc = new QShortcut(QKeySequence("Ctrl+Shift+Z"), this);
    connect(redo_sc, &QShortcut::activated, this, &MainWindow::redo);
    auto* fs_sc = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fs_sc, &QShortcut::activated, this, &MainWindow::toggleFullscreen);
    auto* esc_sc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc_sc, &QShortcut::activated, this, &MainWindow::exitFullscreen);

    startup_mode_ = loadStartupMode();
    applyCurrentTheme();
    updateWelcomeState();
    stack_->setCurrentWidget(welcome_);
    if (status_lbl_) status_lbl_->clear();
    if (hover_lbl_)  hover_lbl_->clear();
    updateWorkspaceChrome();
    showMaximized();
    if (startup_mode_ == "workspace") {
        QTimer::singleShot(0, this, [this] { showWorkspace(); });
    }
}

void MainWindow::buildToolbar() {
    toolbar_ = new QWidget(this);
    auto* lay = new QHBoxLayout(toolbar_);
    lay->setContentsMargins(12, 5, 12, 5);
    lay->setSpacing(0);

    btn_home_ = new WarmButton("Back to Home",   WarmButton::Secondary, toolbar_);
    btn_mesh_ = new WarmButton("Load Mesh",      WarmButton::Primary,   toolbar_);
    btn_diff_ = new WarmButton("Load Diffuse",   WarmButton::Secondary, toolbar_);
    btn_all_  = new WarmButton("Select All",     WarmButton::Secondary, toolbar_);
    btn_none_ = new WarmButton("Deselect All",   WarmButton::Secondary, toolbar_);
    btn_inv_  = new WarmButton("Invert",         WarmButton::Secondary, toolbar_);
    btn_undo_ = new WarmButton("Undo",           WarmButton::Secondary, toolbar_);
    btn_redo_ = new WarmButton("Redo",           WarmButton::Secondary, toolbar_);
    btn_tga_  = new WarmButton("Export TGA",     WarmButton::Secondary, toolbar_);
    btn_png_  = new WarmButton("Export PNG",     WarmButton::Secondary, toolbar_);
    btn_mesh_->setDropKind(WarmButton::MeshDrop);
    btn_diff_->setDropKind(WarmButton::DiffuseDrop);
    settings_btn_ = new WarmButton(QString::fromUtf8("\xE2\x9A\x99"), WarmButton::Secondary, toolbar_);
    QFont sf; sf.setPointSize(14); sf.setBold(true);
    settings_btn_->setFont(sf);

    alpha_btn_ = new QLabel("Alpha: ON", toolbar_);
    alpha_btn_->setAutoFillBackground(true);
    alpha_btn_->setCursor(Qt::PointingHandCursor);
    alpha_btn_->setMargin(6);
    struct AlphaClick : QObject {
        MainWindow* w;
        explicit AlphaClick(MainWindow* w) : w(w) {}
        bool eventFilter(QObject*, QEvent* e) override {
            if (e->type() == QEvent::MouseButtonRelease) w->toggleAlpha();
            return false;
        }
    };
    alpha_btn_->installEventFilter(new AlphaClick(this));

    auto* left_group = new QWidget(toolbar_);
    auto* left_lay = new QHBoxLayout(left_group);
    left_lay->setContentsMargins(0, 0, 0, 0);
    left_lay->setSpacing(6);
    for (auto* b : {btn_home_, btn_mesh_, btn_diff_}) left_lay->addWidget(b);
    left_lay->addSpacing(6);
    left_lay->addWidget(alpha_btn_);

    auto* middle_group = new QWidget(toolbar_);
    auto* middle_lay = new QHBoxLayout(middle_group);
    middle_lay->setContentsMargins(0, 0, 0, 0);
    middle_lay->setSpacing(6);
    for (auto* b : {btn_all_, btn_none_, btn_inv_}) middle_lay->addWidget(b);
    middle_lay->addSpacing(6);
    for (auto* b : {btn_undo_, btn_redo_}) middle_lay->addWidget(b);

    auto* right_group = new QWidget(toolbar_);
    auto* right_lay = new QHBoxLayout(right_group);
    right_lay->setContentsMargins(0, 0, 0, 0);
    right_lay->setSpacing(6);
    right_lay->addWidget(btn_tga_);
    right_lay->addWidget(btn_png_);
    right_lay->addSpacing(8);
    right_lay->addWidget(settings_btn_);

    lay->addWidget(left_group);
    lay->addStretch(1);
    lay->addWidget(middle_group);
    lay->addStretch(1);
    lay->addWidget(right_group);

    connect(btn_home_, &WarmButton::clicked, this, &MainWindow::backToHome);
    connect(btn_mesh_, &WarmButton::clicked, this, &MainWindow::loadMesh);
    connect(btn_diff_, &WarmButton::clicked, this, &MainWindow::loadDiffuse);
    connect(btn_mesh_, &WarmButton::fileDropped, this, &MainWindow::loadMeshFromPath);
    connect(btn_diff_, &WarmButton::fileDropped, this, &MainWindow::loadDiffuseFromPath);
    connect(settings_btn_, &WarmButton::clicked, this, &MainWindow::openSettingsMenuFromToolbar);
    connect(btn_all_,  &WarmButton::clicked, this, &MainWindow::selectAll);
    connect(btn_none_, &WarmButton::clicked, this, &MainWindow::deselectAll);
    connect(btn_inv_,  &WarmButton::clicked, this, &MainWindow::invertSelection);
    connect(btn_undo_, &WarmButton::clicked, this, &MainWindow::undo);
    connect(btn_redo_, &WarmButton::clicked, this, &MainWindow::redo);
    connect(btn_tga_,  &WarmButton::clicked, this, &MainWindow::exportTGA);
    connect(btn_png_,  &WarmButton::clicked, this, &MainWindow::exportPNG);
    btn_undo_->setEnabled(false);
    btn_redo_->setEnabled(false);

    toolbar_->hide();
    // We place the toolbar above the stack by re-arranging the central widget.
    auto* central = new QWidget(this);
    auto* vl = new QVBoxLayout(central);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(toolbar_);
    vl->addWidget(stack_, 1);
    setCentralWidget(central);
}

void MainWindow::buildShapeTreeDock() {
    shape_tree_ = new ShapeTreePanel(this);
    shape_dock_ = new QDockWidget("Shapes", this);
    shape_dock_->setObjectName("shape_tree_dock");
    shape_dock_->setWidget(shape_tree_);
    shape_dock_->setFeatures(QDockWidget::NoDockWidgetFeatures);
    shape_dock_->setAllowedAreas(Qt::RightDockWidgetArea);
    shape_dock_->setTitleBarWidget(new QWidget(shape_dock_));
    addDockWidget(Qt::RightDockWidgetArea, shape_dock_);
    shape_dock_->hide();

    connect(shape_tree_, &ShapeTreePanel::meshSelectionToggleRequested,
            this, &MainWindow::onMeshSelectionToggleRequested);
    connect(shape_tree_, &ShapeTreePanel::islandSelectionToggleRequested,
            this, &MainWindow::onIslandSelectionToggleRequested);
    connect(shape_tree_, &ShapeTreePanel::meshVisibilityToggleRequested,
            this, &MainWindow::onMeshVisibilityToggleRequested);

    connect(shape_tree_, &ShapeTreePanel::zoomOutRequested,
            canvas_, &UVCanvasWidget::zoomOutStep);
    connect(shape_tree_, &ShapeTreePanel::zoomInRequested,
            canvas_, &UVCanvasWidget::zoomInStep);
    connect(shape_tree_, &ShapeTreePanel::zoomFitRequested,
            canvas_, &UVCanvasWidget::zoomFit);
    connect(canvas_, &UVCanvasWidget::zoomChanged,
            shape_tree_, &ShapeTreePanel::setZoomPercent);

    // Hover lockstep between canvas and sidebar.
    connect(canvas_, &UVCanvasWidget::hoverIslandChanged, this,
            [this](int mi, int ii) {
                if (shape_tree_)
                    shape_tree_->refreshHighlights(canvas_->meshes(), mi, ii);
            });
    connect(shape_tree_, &ShapeTreePanel::islandRowHoverChanged, this,
            [this](int mi, int ii) {
                if (canvas_) canvas_->setExternalIslandHover(mi, ii);
                if (shape_tree_)
                    shape_tree_->refreshHighlights(canvas_->meshes(), mi, ii);
            });

    // Focus / leave cleanup: when the canvas drops hover + drag state, drop
    // the sidebar highlight too and replace the hover tooltip with the
    // workspace intro hint (Python `_on_focus_out` -> `_reset_workspace_status`).
    connect(canvas_, &UVCanvasWidget::interactionStateCleared, this, [this] {
        if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes(), -1, -1);
        if (hover_lbl_) hover_lbl_->clear();
        resetWorkspaceStatus();
    });
}

void MainWindow::buildStatusBar() {
    status_lbl_ = new QLabel(QString(), this);
    hover_lbl_  = new QLabel(QString(), this);
    statusBar()->addWidget(status_lbl_, 1);
    statusBar()->addPermanentWidget(hover_lbl_);
}

void MainWindow::applyThemeVisuals(const themes::Theme& t) {
    QPalette p = palette();
    p.setColor(QPalette::Window,     t.bg_deep);
    p.setColor(QPalette::WindowText, t.parchment);
    p.setColor(QPalette::Base,       t.bg_panel);
    p.setColor(QPalette::Text,       t.parchment);
    setPalette(p);

    auto apply_palette = [&](QWidget* w, QColor bg, QColor fg) {
        if (!w) return;
        QPalette pp = w->palette();
        pp.setColor(QPalette::Window,     bg);
        pp.setColor(QPalette::WindowText, fg);
        w->setPalette(pp);
        w->setAutoFillBackground(true);
    };
    apply_palette(toolbar_, t.bg_toolbar, t.parchment);
    apply_palette(alpha_btn_, alpha_on_ ? t.primary : t.surface, alpha_on_ ? t.parchment : t.parchment_dim);

    for (auto* b : {btn_home_, btn_mesh_, btn_diff_, btn_all_, btn_none_, btn_inv_,
                    btn_undo_, btn_redo_, settings_btn_, btn_tga_, btn_png_})
        if (b) b->applyTheme(t);

    welcome_->applyTheme(t);
    canvas_->setThemeColors(t.parchment_faint, t.primary_hi, t.selection_color);
    canvas_->setPreviewColor(t.surface_hi);
    canvas_->setBackgroundColors(t.bg_canvas, QColor(26, 26, 26), QColor(46, 46, 46));
    canvas_->setEmptyStateColors(t.parchment, QColor(t.surface.red(), t.surface.green(), t.surface.blue(), 230));
    if (shape_tree_) {
        shape_tree_->applyTheme(t);
        shape_tree_->refreshHighlights(canvas_->meshes());
    }
}

void MainWindow::applyCurrentTheme() {
    applyThemeVisuals(ThemeManager::instance().current());
}

void MainWindow::updateWelcomeState() {
    if (!welcome_) return;
    welcome_->setLoadedFiles(QFileInfo(mesh_path_).fileName(), QFileInfo(diffuse_path_).fileName());
    welcome_->setWorkspaceButtonHasLoadedContent(!mesh_path_.isEmpty() || !diffuse_path_.isEmpty());
}

void MainWindow::updateWorkspaceChrome() {
    if (shape_dock_) {
        const bool in_workspace = (stack_ && stack_->currentWidget() == canvas_);
        if (in_workspace) shape_dock_->show();
        else              shape_dock_->hide();
    }
}

QString MainWindow::loadStartupMode() const {
    const QString mode = app::loadSetting("startup_mode", "welcome").trimmed().toLower();
    return mode == "workspace" ? "workspace" : "welcome";
}

void MainWindow::saveStartupMode(const QString& mode) const {
    app::saveSetting("startup_mode", mode == "workspace" ? "workspace" : "welcome");
}

void MainWindow::showInfoPopup() {
    auto* dlg = new InfoPopup(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->applyTheme(ThemeManager::instance().current());
    dlg->show();
}

void MainWindow::openSettingsMenuFromToolbar() {
    openSettingsMenu(settings_btn_);
}

void MainWindow::openSettingsMenu(QWidget* anchor) {
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    if (settings_menu_ && settings_menu_->isVisible()) {
        last_settings_anchor_ = anchor;
        last_settings_menu_close_ms_ = now_ms;
        settings_menu_->close();
        settings_menu_.clear();
        return;
    }
    if (anchor && last_settings_anchor_ == anchor && (now_ms - last_settings_menu_close_ms_) < 250) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    settings_menu_ = menu;
    connect(menu, &QObject::destroyed, this, [this] {
        settings_menu_.clear();
    });
    connect(menu, &QMenu::aboutToHide, this, [this, anchor] {
        last_settings_anchor_ = anchor;
        last_settings_menu_close_ms_ = QDateTime::currentMSecsSinceEpoch();
    });

    auto* startup_group = new QActionGroup(menu);
    startup_group->setExclusive(true);

    auto* welcome_act = menu->addAction("Launch to Welcome Screen");
    welcome_act->setCheckable(true);
    welcome_act->setChecked(startup_mode_ != "workspace");
    startup_group->addAction(welcome_act);

    auto* workspace_act = menu->addAction("Launch to Workspace");
    workspace_act->setCheckable(true);
    workspace_act->setChecked(startup_mode_ == "workspace");
    startup_group->addAction(workspace_act);

    menu->addSeparator();
    QAction* theme_act = menu->addAction("Theme...");
    QAction* info_act  = menu->addAction("Info");

    connect(welcome_act, &QAction::triggered, this, [this] {
        startup_mode_ = "welcome";
        saveStartupMode(startup_mode_);
    });
    connect(workspace_act, &QAction::triggered, this, [this] {
        startup_mode_ = "workspace";
        saveStartupMode(startup_mode_);
    });
    connect(theme_act, &QAction::triggered, this, [this, anchor] {
        openThemeMenu(anchor);
    });
    connect(info_act, &QAction::triggered, this, &MainWindow::showInfoPopup);

    const auto& t = ThemeManager::instance().current();
    menu->setStyleSheet(QString(
        "QMenu { background:%1; color:%2; border:1px solid %3; }"
        "QMenu::item { padding:6px 20px 6px 18px; }"
        "QMenu::item:selected { background:%4; color:%2; }"
        "QMenu::separator { height:1px; background:%3; margin:4px 10px; }")
        .arg(t.surface.name(QColor::HexRgb),
             t.parchment.name(QColor::HexRgb),
             t.rule.name(QColor::HexRgb),
             t.surface_hi.name(QColor::HexRgb)));

    if (anchor) menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
    else        menu->popup(QCursor::pos());
}

void MainWindow::toggleAlpha() {
    alpha_on_ = !alpha_on_;
    canvas_->setAlphaEnabled(alpha_on_);
    alpha_btn_->setText(alpha_on_ ? "Alpha: ON" : "Alpha: OFF");
    applyCurrentTheme();
}

void MainWindow::showWorkspace() {
    stack_->setCurrentWidget(canvas_);
    toolbar_->show();
    updateWorkspaceChrome();
    resetWorkspaceStatus();
    // Defer a re-fit after the layout ripple from showing the toolbar + dock
    // so the canvas's width/height reflect the real workspace viewport. The
    // canvas's own showEvent also runs a fit on first appearance; running it
    // here too costs nothing and covers the case where the user toggles
    // back-and-forth via "Open in Workspace" (showEvent only fires on the
    // hidden→visible transition).
    QTimer::singleShot(50, this, [this] {
        if (canvas_) canvas_->zoomFit();
    });
}

void MainWindow::resetWorkspaceStatus() {
    const QImage& img = canvas_->diffuse();
    const int W = img.isNull() ? 1024 : img.width();
    const int H = img.isNull() ? 1024 : img.height();
    status_lbl_->setText(
        QString("%1\u00D7%2 UV space  \u00B7  "
                "Click/drag to select  \u00B7  Scroll to zoom  \u00B7  "
                "Space+drag to pan  \u00B7  "
                "Ctrl+Z Undo  \u00B7  Ctrl+Shift+Z Redo  \u00B7  F11 fullscreen")
            .arg(W).arg(H));
}
void MainWindow::backToHome() {
    // Full reset to a fresh welcome session. Matches Python `_go_home`: the
    // workspace owns mesh data, diffuse, selection history, and status text,
    // and every one of those has to leave with the workspace or the welcome
    // screen shows stale fragments (status bar hint, "Open in Workspace"
    // button, shape-tree rows, hover tooltips) while the user is trying to
    // start over.
    stack_->setCurrentWidget(welcome_);
    toolbar_->hide();

    // Drop the loaded scene.
    canvas_->setMeshes({});
    canvas_->setDiffuse(QImage());
    if (shape_tree_) shape_tree_->rebuild(canvas_->meshes());

    // Drop undo / redo history (tied to the scene that no longer exists).
    undo_stack_.clear();
    redo_stack_.clear();
    updateUndoRedoButtons();

    // Drop workspace-era status/hover text so the welcome screen isn't
    // displaying "UV space · Click/drag to select..." at the bottom.
    if (status_lbl_) status_lbl_->clear();
    if (hover_lbl_)  hover_lbl_->clear();

    // Reset welcome-screen state and the "Open in Workspace" hook — neither
    // asset is loaded anymore.
    mesh_path_.clear();
    diffuse_path_.clear();

    // Normalize the alpha-checker toggle back to its default ON state so a
    // fresh session starts with the same transparency preview the first one
    // did (otherwise the label says "Alpha: ON" while the underlying state
    // is a stale OFF from the previous session, or vice-versa).
    alpha_on_ = true;
    if (canvas_) canvas_->setAlphaEnabled(true);
    if (alpha_btn_) alpha_btn_->setText("Alpha: ON");
    applyCurrentTheme();
    updateWelcomeState();
    updateWorkspaceChrome();
}

void MainWindow::loadMesh() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Load Mesh", QString(),
        "Mesh Files (*.nif *.obj);;NIF (*.nif);;OBJ (*.obj)");
    if (path.isEmpty()) return;
    loadMeshFromPath(path);
}

bool MainWindow::loadMeshFromPath(const QString& path) {
    if (!is_mesh_file(path)) return false;

    // Snapshot whether the workspace is already on screen so we can preserve
    // its status-bar hint after an in-workspace reload.
    const bool workspace_shown = (stack_->currentWidget() == canvas_);

    status_lbl_->setText("Loading mesh...");
    QApplication::processEvents();

    std::vector<geom::Mesh> meshes;
    try {
        QFileInfo fi(path);
        if (fi.suffix().compare("nif", Qt::CaseInsensitive) == 0) {
            parsers::NiflyParser p;
            meshes = p.parse(path, [this](float, const QString& msg) {
                status_lbl_->setText(msg);
                QApplication::processEvents();
            });
        } else {
            parsers::OBJParser p;
            meshes = p.parse(path);
        }
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Load failed", ex.what());
        if (workspace_shown) resetWorkspaceStatus();
        else if (status_lbl_) status_lbl_->clear();
        return false;
    }

    if (meshes.empty()) {
        // Matches Python `_load_mesh` line 3674 — the only post-parse failure
        // message: the parser drops meshes that yielded no UV triangles, so an
        // empty vector always means "no textured geometry".
        QMessageBox::warning(this, "No UV Data",
                             "The mesh contains no UV coordinates.");
        if (workspace_shown) resetWorkspaceStatus();
        else if (status_lbl_) status_lbl_->clear();
        return false;
    }

    int total_tris = 0;
    for (const auto& m : meshes) total_tris += int(m.triangles.size());
    canvas_->setMeshes(std::move(meshes));
    mesh_path_ = path;
    undo_stack_.clear();
    redo_stack_.clear();
    updateUndoRedoButtons();
    if (shape_tree_) shape_tree_->rebuild(canvas_->meshes());
    updateWorkspaceChrome();

    const QString loaded_msg = QString("Mesh loaded - %1 triangles").arg(total_tris);
    updateWelcomeState();
    if (workspace_shown) resetWorkspaceStatus();
    else                                status_lbl_->setText(loaded_msg);
    return true;
}

void MainWindow::loadDiffuse() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Load Diffuse Texture", QString(),
        "Images (*.png *.tga *.dds *.jpg *.jpeg *.bmp)");
    if (path.isEmpty()) return;
    loadDiffuseFromPath(path);
}

bool MainWindow::loadDiffuseFromPath(const QString& path) {
    if (!is_diffuse_file(path)) return false;

    // Snapshot whether the workspace is already on screen so we can preserve
    // its status-bar hint after an in-workspace reload.
    const bool workspace_shown = (stack_->currentWidget() == canvas_);

    status_lbl_->setText("Decoding texture...");
    QApplication::processEvents();

    QImage img;
    try {
        QFileInfo fi(path);
        if (fi.suffix().compare("dds", Qt::CaseInsensitive) == 0) {
            img = codec::load_dds_image(path);
        } else if (fi.suffix().compare("tga", Qt::CaseInsensitive) == 0) {
            QImageReader r(path);
            r.setFormat("tga");
            img = r.read();
        } else {
            img.load(path);
        }
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Texture load failed", ex.what());
        if (workspace_shown) resetWorkspaceStatus();
        else if (status_lbl_) status_lbl_->clear();
        return false;
    }

    if (img.isNull()) {
        QMessageBox::warning(this, "Failed", "Could not decode image.");
        if (workspace_shown) resetWorkspaceStatus();
        else if (status_lbl_) status_lbl_->clear();
        return false;
    }
    canvas_->setDiffuse(img.convertToFormat(QImage::Format_RGBA8888));
    diffuse_path_ = path;
    updateWorkspaceChrome();

    const QString loaded_msg = QString("Loaded texture %1×%2").arg(img.width()).arg(img.height());
    updateWelcomeState();
    if (workspace_shown) resetWorkspaceStatus();
    else                               status_lbl_->setText(loaded_msg);
    return true;
}

void MainWindow::selectAll() {
    saveSelectionSnapshot();
    for (auto& m : canvas_->meshes_mut())
        for (auto& t : m.triangles) t.selected = true;
    canvas_->refreshSelection();
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}
void MainWindow::deselectAll() {
    saveSelectionSnapshot();
    for (auto& m : canvas_->meshes_mut())
        for (auto& t : m.triangles) t.selected = false;
    canvas_->refreshSelection();
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}
void MainWindow::invertSelection() {
    saveSelectionSnapshot();
    for (auto& m : canvas_->meshes_mut())
        for (auto& t : m.triangles) t.selected = !t.selected;
    canvas_->refreshSelection();
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}

void MainWindow::onMeshSelectionToggleRequested(int mesh_idx) {
    auto& meshes = canvas_->meshes_mut();
    if (mesh_idx < 0 || mesh_idx >= int(meshes.size())) return;
    saveSelectionSnapshot();
    auto& m = meshes[mesh_idx];
    bool any_sel = false;
    for (const auto& t : m.triangles) if (t.selected) { any_sel = true; break; }
    for (auto& t : m.triangles) t.selected = !any_sel;
    canvas_->refreshSelection();
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}

void MainWindow::onIslandSelectionToggleRequested(int mesh_idx, int island_idx) {
    auto& meshes = canvas_->meshes_mut();
    if (mesh_idx < 0 || mesh_idx >= int(meshes.size())) return;
    saveSelectionSnapshot();
    auto& m = meshes[mesh_idx];
    bool any_sel = false;
    for (const auto& t : m.triangles) {
        if (t.island_id.has_value() && *t.island_id == island_idx && t.selected) {
            any_sel = true;
            break;
        }
    }
    for (auto& t : m.triangles) {
        if (t.island_id.has_value() && *t.island_id == island_idx)
            t.selected = !any_sel;
    }
    canvas_->refreshSelection();
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}

void MainWindow::onMeshVisibilityToggleRequested(int mesh_idx) {
    auto& meshes = canvas_->meshes_mut();
    if (mesh_idx < 0 || mesh_idx >= int(meshes.size())) return;
    meshes[mesh_idx].visible = !meshes[mesh_idx].visible;
    canvas_->rebuildSpatialGrids();
    canvas_->refreshSelection(); // rebuilds wireframe (hidden meshes excluded)
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}

void MainWindow::onCanvasSelectionChanged() {
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}

std::vector<std::vector<bool>> MainWindow::captureSelectionSnapshot() const {
    std::vector<std::vector<bool>> snap;
    const auto& meshes = canvas_->meshes();
    snap.reserve(meshes.size());
    for (const auto& m : meshes) {
        std::vector<bool> row;
        row.reserve(m.triangles.size());
        for (const auto& t : m.triangles) row.push_back(t.selected);
        snap.push_back(std::move(row));
    }
    return snap;
}

void MainWindow::saveSelectionSnapshot() {
    undo_stack_.push_back(captureSelectionSnapshot());
    if (int(undo_stack_.size()) > kMaxUndoStates)
        undo_stack_.erase(undo_stack_.begin());
    redo_stack_.clear();
    updateUndoRedoButtons();
}

void MainWindow::updateUndoRedoButtons() {
    if (btn_undo_) btn_undo_->setEnabled(!undo_stack_.empty());
    if (btn_redo_) btn_redo_->setEnabled(!redo_stack_.empty());
}

void MainWindow::restoreSelectionSnapshot(const std::vector<std::vector<bool>>& state) {
    auto& meshes = canvas_->meshes_mut();
    for (size_t mi = 0; mi < state.size() && mi < meshes.size(); ++mi) {
        auto& m = meshes[mi];
        const auto& row = state[mi];
        for (size_t ti = 0; ti < row.size() && ti < m.triangles.size(); ++ti)
            m.triangles[ti].selected = row[ti];
    }
    canvas_->refreshSelection();
    if (shape_tree_) shape_tree_->refreshHighlights(canvas_->meshes());
}

void MainWindow::undo() {
    if (undo_stack_.empty()) return;
    redo_stack_.push_back(captureSelectionSnapshot());
    auto state = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    restoreSelectionSnapshot(state);
    updateUndoRedoButtons();
}

void MainWindow::redo() {
    if (redo_stack_.empty()) return;
    undo_stack_.push_back(captureSelectionSnapshot());
    auto state = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    restoreSelectionSnapshot(state);
    updateUndoRedoButtons();
}

QImage MainWindow::buildExportImage() {
    const QImage& diffuse = canvas_->diffuse();
    const auto& meshes = canvas_->meshes();

    const int w = diffuse.isNull() ? 1024 : diffuse.width();
    const int h = diffuse.isNull() ? 1024 : diffuse.height();

    QImage out(w, h, QImage::Format_RGBA8888);
    out.fill(Qt::transparent);

    // Mask pass: rasterize selected triangles into an alpha mask, then compose the
    // diffuse color into the output where the mask is set. Export uses hard-edged
    // fills rather than antialiased triangle edges so the cutout stays solid and
    // does not inherit faint UV seam lines around the selected area.
    QImage mask(w, h, QImage::Format_Grayscale8);
    mask.fill(0);
    {
        QPainter mp(&mask);
        mp.setRenderHint(QPainter::Antialiasing, false);
        mp.setPen(Qt::NoPen);
        mp.setBrush(QColor(255, 255, 255));
        for (const auto& m : meshes) {
            if (!m.visible) continue;
            for (const auto& t : m.triangles) {
                if (!t.selected) continue;
                QPointF pts[3] = {
                    QPointF(t.uv[0].u * w, t.uv[0].v * h),
                    QPointF(t.uv[1].u * w, t.uv[1].v * h),
                    QPointF(t.uv[2].u * w, t.uv[2].v * h),
                };
                mp.drawPolygon(pts, 3);
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        const uint8_t* mline = mask.constScanLine(y);
        uint8_t* dline = out.scanLine(y);
        const uint8_t* sline = diffuse.isNull() ? nullptr : diffuse.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            uint8_t m = mline[x];
            if (!m) continue;
            if (sline) {
                dline[x * 4 + 0] = sline[x * 4 + 0];
                dline[x * 4 + 1] = sline[x * 4 + 1];
                dline[x * 4 + 2] = sline[x * 4 + 2];
                dline[x * 4 + 3] = uint8_t((int(sline[x * 4 + 3]) * m) / 255);
            } else {
                dline[x * 4 + 0] = 255;
                dline[x * 4 + 1] = 255;
                dline[x * 4 + 2] = 255;
                dline[x * 4 + 3] = m;
            }
        }
    }
    return out;
}

void MainWindow::exportTGA() { doExport("tga"); }
void MainWindow::exportPNG() { doExport("png"); }

void MainWindow::doExport(const QString& fmt) {
    // Match Python `_do_export` (lines 4763-4805): validate selection, require
    // diffuse, build default filename from the diffuse stem, anchor the save
    // dialog in the exe directory, rasterize, write, then report the full
    // written path in the status bar.
    bool any_selected = false;
    for (const auto& m : canvas_->meshes()) {
        for (const auto& t : m.triangles) {
            if (t.selected) { any_selected = true; break; }
        }
        if (any_selected) break;
    }
    if (!any_selected) {
        QMessageBox::warning(this, "Nothing Selected",
                             "Select at least one triangle before exporting.");
        return;
    }
    if (canvas_->diffuse().isNull()) {
        QMessageBox::warning(this, "No Diffuse", "Load a diffuse texture to export.");
        return;
    }

    const QString exe_dir = QCoreApplication::applicationDirPath();
    QString default_name;
    if (!diffuse_path_.isEmpty()) {
        default_name = QString("%1_cutout.%2")
            .arg(QFileInfo(diffuse_path_).completeBaseName(), fmt);
    } else {
        default_name = QString("Diffuse_Cutouts.%1").arg(fmt);
    }

    const QString fmt_upper = fmt.toUpper();
    const QString filter = QString("%1 files (*.%2);;All files (*.*)")
        .arg(fmt_upper, fmt);
    const QString path = QFileDialog::getSaveFileName(
        this,
        QString("Save %1 cutout").arg(fmt_upper),
        QDir(exe_dir).filePath(default_name),
        filter);
    if (path.isEmpty()) return;

    status_lbl_->setText(QString("Exporting %1...").arg(fmt_upper));
    QApplication::processEvents();

    QImage img = buildExportImage();
    bool ok;
    if (fmt == "tga") ok = codec::write_tga(path, img);
    else              ok = img.save(path, "PNG");

    if (!ok) {
        QMessageBox::critical(this, "Export",
                              QString("Failed to write %1.").arg(fmt_upper));
        return;
    }
    status_lbl_->setText(QString("Exported  ·  %1").arg(path));
}

void MainWindow::openThemeMenu() {
    QWidget* anchor = nullptr;
    if (stack_ && stack_->currentWidget() == canvas_) anchor = settings_btn_;
    else if (welcome_)                                anchor = welcome_->settingsButton();
    openThemeMenu(anchor);
}

void MainWindow::openThemeMenu(QWidget* anchor) {
    if (!theme_dialog_) {
        theme_dialog_ = new ThemePickerDialog(this);
        connect(theme_dialog_, &ThemePickerDialog::themePreviewed, this, [this](const QString& name) {
            const auto& mgr = ThemeManager::instance();
            const auto& t = mgr.get(name);
            if (!t.name.isEmpty()) {
                QSignalBlocker blocker(theme_dialog_);
                applyThemeVisuals(t);
                theme_dialog_->applyTheme(t);
            }
        });
        connect(theme_dialog_, &ThemePickerDialog::themeSelected, this, &MainWindow::applyTheme);
        connect(theme_dialog_, &ThemePickerDialog::themeSelectionCanceled, this, [this](const QString& revert_name) {
            applyTheme(revert_name);
        });
    }
    theme_dialog_->applyTheme(ThemeManager::instance().current());
    theme_dialog_->popupBelow(anchor);
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) showNormal();
    else                showFullScreen();
}
void MainWindow::exitFullscreen() {
    if (isFullScreen()) showNormal();
}

void MainWindow::applyTheme(const QString& name) {
    ThemeManager::instance().setCurrent(name);
    applyCurrentTheme();
    if (theme_dialog_) theme_dialog_->applyTheme(ThemeManager::instance().current());
}

} // namespace uvc::ui
