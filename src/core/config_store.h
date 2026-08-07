#pragma once

#include "models.h"

#include <QString>
#include <QStringList>

namespace poetry {

class ConfigStore {
public:
    ConfigStore();
    explicit ConfigStore(const QString &rootOverride);

    QString rootPath() const;
    QString configPath() const;
    QString cachePath() const;
    QString wallpaperCachePath() const;
    QString logPath() const;

    bool load(AppConfig &config, QStringList *warnings = nullptr) const;
    bool save(const AppConfig &config, QString *error = nullptr) const;
    bool backupAndReset(QString *error = nullptr) const;

    static AppConfig defaultConfig();

private:
    QString m_rootPath;
};

} // namespace poetry
