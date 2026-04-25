#pragma once

#include <QColor>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace uvc::themes {

struct Theme {
    QString name;
    QString desc;
    QString font_main;
    QString font_title;

    QColor bg_deep, bg_mid, bg_panel, bg_toolbar, bg_canvas;
    QColor surface, surface_hi, surface_act;
    QColor parchment, parchment_dim, parchment_faint;
    QColor primary, primary_hi, primary_act;
    QColor secondary, secondary_hi;
    QColor tertiary, tertiary_txt;
    QColor rule, rule_dim;
    QColor selection_color;
    QColor canvas_wire, canvas_hover;
    QVector<QColor> sel_colors;
};

class ThemeManager {
public:
    // Loads themes.json from the application's Qt resource system on first call.
    static ThemeManager& instance();

    const QStringList& names() const { return names_; }
    const Theme&       get(const QString& name) const;
    const Theme&       current() const { return get(current_name_); }

    void setCurrent(const QString& name);
    QString currentName() const { return current_name_; }

    // Persisted via the app's local settings.ini file in the executable folder.
    QString loadPreferredName() const;
    void    savePreferredName(const QString& name) const;

    // Categorized ordering for the theme picker's section layout.
    const QVector<QPair<QString, QStringList>>& categoryOrder() const { return category_order_; }

private:
    ThemeManager();
    void load();
    void load_category_order();

    QMap<QString, Theme> themes_;
    QStringList          names_;
    QString              current_name_;
    QVector<QPair<QString, QStringList>> category_order_;
};

} // namespace uvc::themes
