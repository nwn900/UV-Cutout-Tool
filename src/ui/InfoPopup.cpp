#include "InfoPopup.h"

#include "../themes/Theme.h"

#include <QKeyEvent>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace uvc::ui {

InfoPopup::InfoPopup(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Info");
    resize(560, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget(scroll);
    auto* lay  = new QVBoxLayout(body);
    lay->setContentsMargins(20, 15, 20, 15);
    lay->setSpacing(0);

    auto add_title = [&](const QString& text, int pt, bool bold, bool italic,
                         int pad_top, int pad_bot) {
        auto* l = new QLabel(text, body);
        QFont f; f.setPointSize(pt); f.setBold(bold); f.setItalic(italic);
        f.setFamily("Georgia");
        l->setFont(f);
        l->setWordWrap(true);
        l->setObjectName("info_label");
        l->setAlignment(Qt::AlignLeft);
        if (pad_top)  lay->addSpacing(pad_top);
        lay->addWidget(l);
        if (pad_bot)  lay->addSpacing(pad_bot);
    };

    auto add_rich = [&](const QString& html, int pt, bool bold, bool italic,
                        int pad_top, int pad_bot) {
        auto* l = new QLabel(body);
        QFont f; f.setPointSize(pt); f.setBold(bold); f.setItalic(italic);
        f.setFamily("Georgia");
        l->setFont(f);
        l->setWordWrap(true);
        l->setObjectName("info_label");
        l->setAlignment(Qt::AlignLeft);
        l->setTextFormat(Qt::RichText);
        l->setTextInteractionFlags(Qt::TextBrowserInteraction);
        l->setOpenExternalLinks(true);
        l->setText(html);
        if (pad_top) lay->addSpacing(pad_top);
        lay->addWidget(l);
        if (pad_bot) lay->addSpacing(pad_bot);
    };

    auto title_lbl = new QLabel("UV Cutout Tool v1.1.1", body);
    { QFont f; f.setPointSize(14); f.setBold(true); f.setFamily("Georgia");
      title_lbl->setFont(f); title_lbl->setAlignment(Qt::AlignHCenter); }
    lay->addWidget(title_lbl);
    lay->addSpacing(15);

    add_title("What is this?", 11, true, false, 0, 5);
    add_title(
        "A tool for quickly creating texture cutouts from NIF mesh UV layouts. "
        "Load one or more meshes with a matching texture, visualize UV islands, "
        "select triangles, and export just the texture areas you need.",
        10, false, false, 0, 15);

    add_title("Best For:", 11, true, false, 0, 5);
    add_title(
        "\u2022 Armor and clothing pieces\n"
        "\u2022 Weapons and props\n"
        "\u2022 Texture sets shared by several NIF meshes\n"
        "\u2022 Quick isolation of specific UV islands",
        10, false, false, 0, 15);

    add_title("Multiple NIFs:", 11, true, false, 0, 5);
    add_title(
        "You can load several NIF meshes into the same workspace when they share "
        "one diffuse texture. Additional NIFs are appended to the current scene. "
        "When more than one NIF is loaded, the Shapes panel groups each mesh's "
        "shapes under the source file name so armor parts, weapon pieces, and "
        "matching accessory meshes stay organized.",
        10, false, false, 0, 15);

    add_title("Limitations:", 11, true, false, 0, 5);
    add_title(
        "\u2022 Seamless/tileable textures \u2014 UVs outside the 0-1 range may not render correctly.\n\n"
        "\u2022 Multiple materials \u2014 NIFs with different textures per shape will only load the first diffuse selected.\n\n"
        "\u2022 Complex terrain \u2014 Large meshes with overlapping UVs or multiple texture layers.",
        10, false, false, 0, 15);

    add_title("Workflow:", 11, true, false, 0, 5);
    add_title(
        "1. Load one or more NIF meshes or drag them into the app\n"
        "2. Load a diffuse texture (PNG, TGA, DDS, JPG, JPEG, or BMP) or drag it into the app\n"
        "3. You can also drag and drop meshes and a texture at the same time\n"
        "4. Click or drag to select UV islands\n"
        "5. Export selection as TGA or PNG\n"
        "6. Mask/edit in Photoshop",
        10, false, false, 0, 20);

    add_title("Controls:", 11, true, false, 0, 5);
    add_title(
        "\u2022 Click island \u2014 toggle selection\n"
        "\u2022 Click+drag \u2014 box select islands\n"
        "\u2022 Space+drag \u2014 pan view\n"
        "\u2022 Right drag \u2014 pan view (when zoomed in)\n"
        "\u2022 Scroll \u2014 zoom",
        10, false, false, 0, 20);

    add_title("Credits & Libraries:", 11, true, false, 0, 5);
    add_rich(
        "\u2022 <b>Qt 6</b> \u2014 UI framework by The Qt Company Ltd. and contributors. "
        "Used for the desktop application, widgets, and OpenGL integration. "
        "Open-source Qt is available under LGPLv3 / GPLv3 terms. "
        "<a href=\"https://doc.qt.io/qt-6/licensing.html\">Qt licensing</a><br><br>"
        "\u2022 <b>DirectXTex</b> \u2014 texture-processing library by Microsoft. "
        "It informed validation and reference work for the app's DDS BC1/BC2/BC3/BC4/BC5/BC7 decoder behavior. "
        "MIT License. Credits include Chuck Walbourn and Microsoft contributors. "
        "<a href=\"https://github.com/microsoft/DirectXTex\">DirectXTex</a><br><br>"
        "\u2022 <b>NiflyDLL / PyNifly / nifly</b> \u2014 NIF mesh loading relies on NiflyDLL from the PyNifly project, "
        "which wraps nifly and related NIF code. PyNifly is maintained by BadDogSkyrim; nifly is maintained by ousnius. "
        "These upstream projects are GPL-3.0 licensed. Based on the PyNifly project documentation, additional credit is due to "
        "Ousnius and the BodySlide / Outfit Studio team for the nifly layer. "
        "<a href=\"https://github.com/BadDogSkyrim/PyNifly\">PyNifly</a> | "
        "<a href=\"https://github.com/ousnius/nifly\">nifly</a>",
        9, false, false, 0, 20);

    add_title("Created by IconicDeath", 10, false, false, 0, 2);

    auto* link = new QLabel(
        "<a href=\"https://www.nexusmods.com/profile/IconicDeath\">Nexus Mods Profile</a>",
        body);
    { QFont f; f.setPointSize(10); f.setBold(true); f.setFamily("Georgia");
      link->setFont(f); }
    link->setTextFormat(Qt::RichText);
    link->setOpenExternalLinks(true);
    link->setTextInteractionFlags(Qt::TextBrowserInteraction);
    link->setCursor(Qt::PointingHandCursor);
    lay->addWidget(link);
    lay->addSpacing(10);

    add_title("Built with Claude + Codex + OpenCode AI", 9, false, true, 0, 0);

    lay->addStretch();
    scroll->setWidget(body);
    root->addWidget(scroll);
}

void InfoPopup::applyTheme(const themes::Theme& t) {
    QPalette p = palette();
    p.setColor(QPalette::Window, t.surface);
    p.setColor(QPalette::Base,   t.surface);
    p.setColor(QPalette::Text,   t.parchment);
    p.setColor(QPalette::WindowText, t.parchment);
    setPalette(p);
    setAutoFillBackground(true);

    for (auto* l : findChildren<QLabel*>()) {
        QPalette lp = l->palette();
        lp.setColor(QPalette::Window, t.surface);
        lp.setColor(QPalette::WindowText, t.parchment);
        l->setPalette(lp);
        l->setAutoFillBackground(true);
    }
    for (auto* s : findChildren<QScrollArea*>()) {
        if (s->viewport()) {
            QPalette sp = s->viewport()->palette();
            sp.setColor(QPalette::Window, t.surface);
            s->viewport()->setPalette(sp);
            s->viewport()->setAutoFillBackground(true);
        }
    }
}

void InfoPopup::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { close(); return; }
    QDialog::keyPressEvent(e);
}

} // namespace uvc::ui
