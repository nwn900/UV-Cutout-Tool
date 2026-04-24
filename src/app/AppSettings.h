#pragma once

#include <QString>

namespace uvc::app {

// Local settings file written in the executable launch folder.
QString settingsFilePath();
QString loadSetting(const QString& key, const QString& fallback = {});
void    saveSetting(const QString& key, const QString& value);

} // namespace uvc::app
