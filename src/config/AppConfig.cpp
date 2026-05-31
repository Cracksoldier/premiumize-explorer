#include "AppConfig.hpp"

#include <QStandardPaths>

AppConfig& AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig()
    : settings_(QSettings::IniFormat,
                QSettings::UserScope,
                "premiumize-explorer",
                "premiumize-explorer")
{}

QString AppConfig::apiKey() const
{
    return settings_.value("auth/api_key").toString();
}

void AppConfig::setApiKey(const QString& key)
{
    settings_.setValue("auth/api_key", key);
    settings_.sync();
}

QString AppConfig::lastLocalPath() const
{
    const auto fallback = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return settings_.value("ui/last_local_path", fallback).toString();
}

void AppConfig::setLastLocalPath(const QString& path)
{
    settings_.setValue("ui/last_local_path", path);
    settings_.sync();
}

QByteArray AppConfig::windowGeometry() const
{
    return settings_.value("ui/window_geometry").toByteArray();
}

void AppConfig::setWindowGeometry(const QByteArray& geom)
{
    settings_.setValue("ui/window_geometry", geom);
    settings_.sync();
}

QByteArray AppConfig::splitterSizes() const
{
    return settings_.value("ui/splitter_sizes").toByteArray();
}

void AppConfig::setSplitterSizes(const QByteArray& sizes)
{
    settings_.setValue("ui/splitter_sizes", sizes);
    settings_.sync();
}

bool AppConfig::darkModeEnabled() const
{
    return settings_.value("ui/dark_mode", false).toBool();
}

void AppConfig::setDarkModeEnabled(bool dark)
{
    settings_.setValue("ui/dark_mode", dark);
    settings_.sync();
}

bool AppConfig::isConfigured() const
{
    return !apiKey().isEmpty();
}
