#include "MainWindow.h"
#include "ExportDialog.h"
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
#include <QIcon>
#include <QImageReader>
#include <QImageWriter>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QShortcut>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace uvc::ui {

using themes::ThemeManager;

namespace {

bool is_mesh_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "nif";
}

bool is_diffuse_file(const QString& path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == "png" || ext == "tga" || ext == "dds"
        || ext == "jpg" || ext == "jpeg" || ext == "bmp";
}

QIcon make_gear_icon(const QColor& color) {
    QPixmap pm(24, 24);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(12, 12);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    for (int i = 0; i < 8; ++i) {
        p.save();
        p.rotate(i * 45.0);
        p.drawRoundedRect(QRectF(-2.0, -11.0, 4.0, 5.0), 1.0, 1.0);
        p.restore();
    }
    p.drawEllipse(QPointF(0, 0), 7.0, 7.0);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.drawEllipse(QPointF(0, 0), 3.0, 3.0);
    return QIcon(pm);
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

    welcome_->acceptDrops();
    connect(welcome_, &WelcomeWidget::loadMeshRequested,    this, &MainWindow::loadMesh);
    connect(welcome_, &WelcomeWidget::loadDiffuseRequested, this, &MainWindow::loadDiffuse);
    connect(welcome_, &WelcomeWidget::openWorkspaceRequested, this, &MainWindow::showWorkspace);
    connect(welcome_, &WelcomeWidget::settingsRequested, this, [this] {
        if (welcome_ && welcome_->settingsButton()) openSettingsMenu(welcome_->settingsButton());
    });
    connect(welcome_, &WelcomeWidget::meshFileDropped, this, &MainWindow::loadMeshFromPath);
    connect(welcome_, &WelcomeWidget::diffuseFileDropped, this, &MainWindow::loadDiffuseFromPath);
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
    toolbar_->setObjectName("workspaceToolbar");
    auto* lay = new QHBoxLayout(toolbar_);
    lay->setContentsMargins(12, 5, 12, 5);
    lay->setSpacing(0);

    btn_home_ = new WarmButton("Back to Home",   WarmButton::Secondary, toolbar_);
    btn_mesh_ = new WarmButton("Load Mesh",      WarmButton::Primary,   toolbar_);
    btn_diff_ = new WarmButton("Load Diffuse",   WarmButton::Secondary, toolbar_);
    btn_all_  = new WarmButton("Select All",     WarmButton::Secondary, toolbar_);
    btn_none_ = new WarmButton("Deselect All",   WarmButton::Secondary, toolbar_);
    btn_inv_  = new WarmButton("Invert",         WarmButton::Secondary, toolbar_);
    btn_undo_   = new WarmButton("Undo",     WarmButton::Secondary, toolbar_);
    btn_redo_   = new WarmButton("Redo",     WarmButton::Secondary, toolbar_);
    btn_export_ = new WarmButton("Export…",  WarmButton::Secondary, toolbar_);
    btn_mesh_->setDropKind(WarmButton::MeshDrop);
    btn_diff_->setDropKind(WarmButton::DiffuseDrop);
    settings_btn_ = new WarmButton(QString(), WarmButton::Secondary, toolbar_);
    settings_btn_->setToolTip("Settings");
    settings_btn_->setAccessibleName("Settings");
    settings_btn_->setIconSize(QSize(18, 18));

    alpha_btn_ = new WarmButton("Alpha: ON", WarmButton::Primary, toolbar_);

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
    right_lay->addWidget(btn_export_);
    right_lay->addSpacing(8);
    right_lay->addWidget(settings_btn_);

    const QSize toolbar_button_size(112, 34);
    for (auto* b : {btn_home_, btn_mesh_, btn_diff_, alpha_btn_, btn_all_, btn_none_, btn_inv_,
                    btn_undo_, btn_redo_, btn_export_}) {
        if (b) b->setFixedSize(toolbar_button_size);
    }
    settings_btn_->setFixedSize(34, 34);

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
    connect(alpha_btn_, &WarmButton::clicked, this, &MainWindow::toggleAlpha);
    connect(settings_btn_, &WarmButton::clicked, this, &MainWindow::openSettingsMenuFromToolbar);
    connect(btn_all_,  &WarmButton::clicked, this, &MainWindow::selectAll);
    connect(btn_none_, &WarmButton::clicked, this, &MainWindow::deselectAll);
    connect(btn_inv_,  &WarmButton::clicked, this, &MainWindow::invertSelection);
    connect(btn_undo_,   &WarmButton::clicked, this, &MainWindow::undo);
    connect(btn_redo_,   &WarmButton::clicked, this, &MainWindow::redo);
    connect(btn_export_, &WarmButton::clicked, this, &MainWindow::openExportDialog);
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
    // the sidebar highlight too and restore the workspace intro hint.
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
    if (centralWidget()) {
        centralWidget()->setPalette(p);
        centralWidget()->setAutoFillBackground(true);
    }
    if (stack_) {
        stack_->setPalette(p);
        stack_->setAutoFillBackground(true);
    }

    auto apply_palette = [&](QWidget* w, QColor bg, QColor fg) {
        if (!w) return;
        QPalette pp = w->palette();
        pp.setColor(QPalette::Window,     bg);
        pp.setColor(QPalette::WindowText, fg);
        w->setPalette(pp);
        w->setAutoFillBackground(true);
    };
    apply_palette(toolbar_, t.bg_toolbar, t.parchment);
    if (toolbar_) {
        toolbar_->setStyleSheet(QString(
            "QWidget#workspaceToolbar {"
            " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2);"
            " border-bottom:1px solid %3;"
            "}")
            .arg(t.surface.name(QColor::HexRgb),
                 t.bg_toolbar.name(QColor::HexRgb),
                 t.rule.name(QColor::HexRgb)));
    }
    if (alpha_btn_) alpha_btn_->setStyleKind(alpha_on_ ? WarmButton::Primary : WarmButton::Secondary);
    if (statusBar()) {
        statusBar()->setStyleSheet(QString(
            "QStatusBar {"
            " background:%1;"
            " color:%2;"
            " border-top:1px solid %3;"
            "}"
            "QStatusBar::item { border:none; }")
            .arg(t.bg_toolbar.name(QColor::HexRgb),
                 t.parchment_dim.name(QColor::HexRgb),
                 t.rule.name(QColor::HexRgb)));
    }
    if (status_lbl_ || hover_lbl_) {
        const QString label_qss = QString("QLabel { color:%1; }")
            .arg(t.parchment_dim.name(QColor::HexRgb));
        if (status_lbl_) status_lbl_->setStyleSheet(label_qss);
        if (hover_lbl_)  hover_lbl_->setStyleSheet(label_qss);
    }
    if (shape_dock_) {
        shape_dock_->setStyleSheet(QString(
            "QDockWidget#shape_tree_dock {"
            " background:%1;"
            " border-left:1px solid %2;"
            "}")
            .arg(t.bg_panel.name(QColor::HexRgb),
                 t.rule.name(QColor::HexRgb)));
    }

    for (auto* b : {btn_home_, btn_mesh_, btn_diff_, alpha_btn_, btn_all_, btn_none_, btn_inv_,
                    btn_undo_, btn_redo_, btn_export_, settings_btn_})
        if (b) b->applyTheme(t);
    if (export_dialog_) export_dialog_->applyTheme(t);
    if (settings_btn_) settings_btn_->setIcon(make_gear_icon(t.parchment_dim));

    welcome_->applyTheme(t);
    if (stack_) {
        stack_->style()->unpolish(stack_);
        stack_->style()->polish(stack_);
        stack_->update();
    }
    if (centralWidget()) {
        centralWidget()->style()->unpolish(centralWidget());
        centralWidget()->style()->polish(centralWidget());
        centralWidget()->update();
    }
    update();
    canvas_->setThemeColors(t.canvas_wire, t.canvas_hover, t.selection_color);
    canvas_->setPreviewColor(t.canvas_preview);
    canvas_->setBackgroundColors(t.bg_canvas, QColor(26, 26, 26), QColor(46, 46, 46));
    canvas_->setEmptyStateColors(t.parchment, QColor(t.surface.red(), t.surface.green(), t.surface.blue(), 230));
    if (shape_tree_) {
        shape_tree_->applyTheme(t);
        shape_tree_->refreshHighlights(canvas_->meshes());
    }
    if (theme_dialog_) theme_dialog_->applyTheme(t);
    if (settings_menu_) applySettingsMenuTheme(t);
    for (auto* dlg : findChildren<InfoPopup*>()) {
        if (dlg) dlg->applyTheme(t);
    }
}

void MainWindow::applySettingsMenuTheme(const themes::Theme& t) {
    if (!settings_menu_) return;
    settings_menu_->setStyleSheet(QString(
        "QMenu { background:%1; color:%2; border:1px solid %3; padding:4px; }"
        "QMenu::item { padding:7px 24px 7px 18px; border:1px solid transparent; }"
        "QMenu::item:selected { background:%4; color:%2; border-color:%5; }"
        "QMenu::separator { height:1px; background:%3; margin:5px 10px; }")
        .arg(t.surface.name(QColor::HexRgb),
             t.parchment.name(QColor::HexRgb),
             t.rule.name(QColor::HexRgb),
             t.surface_hi.name(QColor::HexRgb),
             t.secondary.name(QColor::HexRgb)));
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
    const bool in_workspace = (stack_ && stack_->currentWidget() == canvas_);
    if (shape_dock_) {
        if (in_workspace) shape_dock_->show();
        else              shape_dock_->hide();
    }
    if (statusBar()) {
        if (in_workspace) statusBar()->show();
        else              statusBar()->hide();
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

    applySettingsMenuTheme(ThemeManager::instance().current());

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
    // Full reset to a fresh welcome session. The workspace owns mesh data,
    // diffuse, selection history, and status text,
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
    // did (otherwise the button says "Alpha: ON" while the underlying state
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
        "NIF Meshes (*.nif)");
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
        parsers::NiflyParser p;
        meshes = p.parse(path, [this](float, const QString& msg) {
            status_lbl_->setText(msg);
            QApplication::processEvents();
        });
    } catch (const std::exception& ex) {
        QMessageBox::critical(this, "Load failed", ex.what());
        if (workspace_shown) resetWorkspaceStatus();
        else if (status_lbl_) status_lbl_->clear();
        return false;
    }

    if (meshes.empty()) {
        // The parser drops meshes that yielded no UV triangles, so an
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

QImage MainWindow::buildExportImage(ExportColorMode mode, bool alpha) {
    const QImage& diffuse = canvas_->diffuse();
    const auto& meshes = canvas_->meshes();

    const int w = diffuse.isNull() ? 1024 : diffuse.width();
    const int h = diffuse.isNull() ? 1024 : diffuse.height();

    // Rasterize selected triangles into a grayscale mask.
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

    // ── Black & White mask ────────────────────────────────────────────────
    if (mode == ExportColorMode::BlackWhite) {
        QImage out(w, h, QImage::Format_RGBA8888);
        for (int y = 0; y < h; ++y) {
            const uint8_t* mline = mask.constScanLine(y);
            uint8_t* dline = out.scanLine(y);
            for (int x = 0; x < w; ++x) {
                const bool sel = mline[x] != 0;
                dline[x * 4 + 0] = sel ? 255 : 0;
                dline[x * 4 + 1] = sel ? 255 : 0;
                dline[x * 4 + 2] = sel ? 255 : 0;
                // Alpha ON: non-selected = fully transparent (renders black on dark bg).
                // Alpha OFF: everything opaque (hard black/white).
                dline[x * 4 + 3] = (!alpha || sel) ? 255 : 0;
            }
        }
        return out;
    }

    // ── Full Color / Grayscale ────────────────────────────────────────────
    // alpha ON:  selected area keeps diffuse alpha; non-selected = transparent.
    // alpha OFF: selected area is fully opaque; non-selected = opaque black.
    QImage out(w, h, QImage::Format_RGBA8888);
    out.fill(alpha ? Qt::transparent : Qt::black);

    for (int y = 0; y < h; ++y) {
        const uint8_t* mline = mask.constScanLine(y);
        uint8_t* dline = out.scanLine(y);
        const uint8_t* sline = diffuse.isNull() ? nullptr : diffuse.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const uint8_t m = mline[x];
            if (!m) continue; // non-selected: already filled (transparent or black)

            uint8_t sr, sg, sb, sa;
            if (sline) {
                sr = sline[x * 4 + 0];
                sg = sline[x * 4 + 1];
                sb = sline[x * 4 + 2];
                // Alpha is driven purely by the selection mask, not the source
                // texture's own alpha channel (which carries unrelated data like
                // specular intensity and is not meaningful for a cutout export).
                sa = alpha ? m : 255;
            } else {
                sr = sg = sb = 255; // no diffuse → white
                sa = alpha ? m : 255;
            }

            if (mode == ExportColorMode::Grayscale) {
                const uint8_t lum = uint8_t(
                    int(sr) * 299 / 1000 +
                    int(sg) * 587 / 1000 +
                    int(sb) * 114 / 1000);
                dline[x * 4 + 0] = lum;
                dline[x * 4 + 1] = lum;
                dline[x * 4 + 2] = lum;
            } else {
                dline[x * 4 + 0] = sr;
                dline[x * 4 + 1] = sg;
                dline[x * 4 + 2] = sb;
            }
            dline[x * 4 + 3] = sa;
        }
    }
    return out;
}

void MainWindow::openExportDialog() {
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

    if (!export_dialog_) {
        export_dialog_ = new ExportDialog(this);
    }
    export_dialog_->applyTheme(ThemeManager::instance().current());

    // Derive the stem from the loaded diffuse path for the filename preview.
    const QString stem = diffuse_path_.isEmpty()
        ? QString()
        : QFileInfo(diffuse_path_).completeBaseName();
    export_dialog_->setDiffuseStem(stem);

    if (export_dialog_->exec() != QDialog::Accepted) return;

    doExport(export_dialog_->format(),
             export_dialog_->colorMode(),
             export_dialog_->includeAlpha(),
             export_dialog_->pngQuality(),
             export_dialog_->tgaRle());
}

void MainWindow::doExport(const QString& fmt, ExportColorMode mode, bool alpha,
                          int png_quality, bool tga_rle) {
    const QString exe_dir = QCoreApplication::applicationDirPath();
    QString default_name;
    if (!diffuse_path_.isEmpty()) {
        default_name = QString("%1_cutout.%2")
            .arg(QFileInfo(diffuse_path_).completeBaseName(), fmt);
    } else {
        default_name = QString("Diffuse_Cutouts.%1").arg(fmt);
    }

    const QString fmt_upper = fmt.toUpper();
    // Put the selected format first in the filter so the save dialog uses the
    // right extension by default; the "All files" fallback lets users override.
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

    QImage img = buildExportImage(mode, alpha);
    bool ok;
    if (fmt == "tga") {
        ok = codec::write_tga(path, img, tga_rle, alpha);
    } else {
        QImageWriter writer(path, "PNG");
        writer.setQuality(png_quality);
        ok = writer.write(img);
    }

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
}

} // namespace uvc::ui
