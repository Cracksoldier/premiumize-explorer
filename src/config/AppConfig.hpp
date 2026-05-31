#pragma once
#include <QSettings>
#include <QString>

class AppConfig
{
public:
    static AppConfig& instance();

    QString apiKey() const;
    void    setApiKey(const QString& key);

    QString lastLocalPath() const;
    void    setLastLocalPath(const QString& path);

    QByteArray windowGeometry() const;
    void       setWindowGeometry(const QByteArray& geom);

    QByteArray splitterSizes() const;
    void       setSplitterSizes(const QByteArray& sizes);

    bool darkModeEnabled() const;
    void setDarkModeEnabled(bool dark);

    bool isConfigured() const;

private:
    AppConfig();
    QSettings settings_;
};
