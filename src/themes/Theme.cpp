#include "Theme.h"
#include "../app/AppSettings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace uvc::themes {

namespace {
QColor parse_color(const QJsonValue& v) {
    return QColor(v.toString());
}
QVector<QColor> parse_sel_colors(const QJsonArray& arr) {
    QVector<QColor> out;
    out.reserve(arr.size());
    for (const auto& e : arr) {
        const auto tri = e.toArray();
        if (tri.size() == 3) {
            out.push_back(QColor(tri[0].toInt(), tri[1].toInt(), tri[2].toInt()));
        }
    }
    return out;
}
} // namespace

ThemeManager::ThemeManager() { load(); load_category_order(); }

ThemeManager& ThemeManager::instance() {
    static ThemeManager mgr;
    return mgr;
}

void ThemeManager::load() {
    QFile f(QStringLiteral(":/themes/themes.json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray raw = f.readAll();
    f.close();
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) return;
    const QJsonObject root = doc.object();

    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QString key = it.key();
        const QJsonObject obj = it.value().toObject();

        Theme t;
        t.name             = obj.value("name").toString(key);
        t.desc             = obj.value("desc").toString();
        t.font_main        = obj.value("font_main").toString();
        t.font_title       = obj.value("font_title").toString();
        t.bg_deep          = parse_color(obj.value("bg_deep"));
        t.bg_mid           = parse_color(obj.value("bg_mid"));
        t.bg_panel         = parse_color(obj.value("bg_panel"));
        t.bg_toolbar       = parse_color(obj.value("bg_toolbar"));
        t.bg_canvas        = parse_color(obj.value("bg_canvas"));
        t.surface          = parse_color(obj.value("surface"));
        t.surface_hi       = parse_color(obj.value("surface_hi"));
        t.surface_act      = parse_color(obj.value("surface_act"));
        t.parchment        = parse_color(obj.value("parchment"));
        t.parchment_faint  = parse_color(obj.value("parchment_faint"));
        t.primary          = parse_color(obj.value("primary"));
        t.primary_hi       = parse_color(obj.value("primary_hi"));
        t.primary_act      = parse_color(obj.value("primary_act"));
        t.secondary        = parse_color(obj.value("secondary"));
        t.secondary_hi     = parse_color(obj.value("secondary_hi"));
        t.tertiary         = parse_color(obj.value("tertiary"));
        t.tertiary_txt     = parse_color(obj.value("tertiary_txt"));
        t.rule             = parse_color(obj.value("rule"));
        t.rule_dim         = parse_color(obj.value("rule_dim"));
        t.selection_color  = parse_color(obj.value("selection_color"));
        t.canvas_wire      = parse_color(obj.value("canvas_wire"));
        t.canvas_hover     = parse_color(obj.value("canvas_hover"));
        t.canvas_preview  = parse_color(obj.value("canvas_preview"));
        if (!t.canvas_wire.isValid())    t.canvas_wire    = t.parchment_faint;
        if (!t.canvas_hover.isValid())   t.canvas_hover  = t.primary_hi;
        if (!t.canvas_preview.isValid())  t.canvas_preview = t.surface_hi;
        t.sel_colors       = parse_sel_colors(obj.value("sel_colors").toArray());

        themes_.insert(key, t);
        names_.push_back(key);
    }

    const QString preferred = loadPreferredName();
    if (!preferred.isEmpty() && themes_.contains(preferred))
        current_name_ = preferred;
    else if (themes_.contains("Modern"))
        current_name_ = "Modern";
    else if (!names_.isEmpty())
        current_name_ = names_.first();
}

const Theme& ThemeManager::get(const QString& name) const {
    static const Theme empty;
    auto it = themes_.constFind(name);
    if (it == themes_.constEnd()) return empty;
    return it.value();
}

void ThemeManager::setCurrent(const QString& name) {
    if (themes_.contains(name)) {
        current_name_ = name;
        savePreferredName(name);
    }
}

QString ThemeManager::loadPreferredName() const {
    return app::loadSetting("theme");
}

void ThemeManager::load_category_order() {
    QFile f(QStringLiteral(":/themes/theme_order.json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray raw = f.readAll();
    f.close();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) return;
    const QJsonArray arr = doc.array();
    for (const auto& e : arr) {
        const QJsonArray pair = e.toArray();
        if (pair.size() != 2) continue;
        const QString category = pair[0].toString();
        QStringList names;
        for (const auto& n : pair[1].toArray()) names << n.toString();
        category_order_.push_back({category, names});
    }
}

void ThemeManager::savePreferredName(const QString& name) const {
    app::saveSetting("theme", name);
}

} // namespace uvc::themes
