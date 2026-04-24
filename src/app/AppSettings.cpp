#include "AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace uvc::app {

namespace {

QString makeSettingsPath() {
    const QString app_dir = QCoreApplication::applicationDirPath();
    if (!app_dir.isEmpty()) return QDir::cleanPath(QDir(app_dir).absoluteFilePath("settings.ini"));
    return QDir::cleanPath(QDir::current().absoluteFilePath("settings.ini"));
}

QSettings& settings() {
    static QSettings s(makeSettingsPath(), QSettings::IniFormat);
    return s;
}

} // namespace

QString settingsFilePath() {
    return makeSettingsPath();
}

QString loadSetting(const QString& key, const QString& fallback) {
    const QString value = settings().value(key, fallback).toString();
    return value.isEmpty() ? fallback : value;
}

void saveSetting(const QString& key, const QString& value) {
    settings().setValue(key, value);
    settings().sync();
}

} // namespace uvc::app
