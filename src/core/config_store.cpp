#include "config_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace poetry {

namespace {

QColor colorFromJson(const QJsonValue &value, const QColor &fallback) {
    if (!value.isString()) return fallback;
    const QColor color(value.toString());
    return color.isValid() ? color : fallback;
}

QJsonObject colorToJson(const QColor &color) {
    return QJsonObject{{QStringLiteral("value"), color.name(QColor::HexArgb)}};
}

QColor colorObjectToColor(const QJsonValue &value, const QColor &fallback) {
    if (value.isString()) return colorFromJson(value, fallback);
    if (value.isObject()) return colorFromJson(value.toObject().value(QStringLiteral("value")), fallback);
    return fallback;
}

QJsonObject renderToJson(const RenderSettings &render) {
    return QJsonObject{
        {QStringLiteral("placement"), anchorToString(render.anchor)},
        {QStringLiteral("fontFamily"), render.fontFamily},
        {QStringLiteral("fontPointSize"), render.fontPointSize},
        {QStringLiteral("textColor"), colorToJson(render.contentColor)},
        {QStringLiteral("titleScale"), render.titleScale},
        {QStringLiteral("authorScale"), render.authorScale},
        {QStringLiteral("dynastyScale"), render.dynastyScale},
        {QStringLiteral("titleWeight"), render.titleWeight},
        {QStringLiteral("titleColor"), colorToJson(render.titleColor)},
        {QStringLiteral("metadataColor"), colorToJson(render.metadataColor)},
        {QStringLiteral("contentColor"), colorToJson(render.contentColor)},
        {QStringLiteral("contentLineSpacing"), render.contentLineSpacing},
        {QStringLiteral("titleMetadataSpacing"), render.titleMetadataSpacing},
        {QStringLiteral("metadataContentSpacing"), render.metadataContentSpacing},
        {QStringLiteral("metadataInlineSpacing"), render.metadataInlineSpacing},
        {QStringLiteral("panelEnabled"), render.panelEnabled},
        {QStringLiteral("panelColor"), colorToJson(render.panelColor)},
        {QStringLiteral("shadowEnabled"), render.shadowEnabled},
        {QStringLiteral("fitMode"), fitModeToString(render.fitMode)},
        {QStringLiteral("canvasBackgroundColor"), colorToJson(render.canvasBackgroundColor)}
    };
}

void renderFromJson(const QJsonObject &object, RenderSettings &render) {
    if (object.value(QStringLiteral("placement")).isString())
        render.anchor = anchorFromString(object.value(QStringLiteral("placement")).toString());
    if (object.value(QStringLiteral("fontFamily")).isString())
        render.fontFamily = object.value(QStringLiteral("fontFamily")).toString();
    if (object.value(QStringLiteral("fontPointSize")).isDouble())
        render.fontPointSize = qBound(10, object.value(QStringLiteral("fontPointSize")).toInt(), 120);

    const bool hasLegacyTextColor = !object.value(QStringLiteral("textColor")).isUndefined();
    render.textColor = colorObjectToColor(object.value(QStringLiteral("textColor")), render.textColor);
    render.contentColor = colorObjectToColor(object.value(QStringLiteral("contentColor")),
                                             hasLegacyTextColor ? render.textColor : render.contentColor);
    render.textColor = render.contentColor;
    render.titleColor = colorObjectToColor(object.value(QStringLiteral("titleColor")), render.titleColor);
    render.metadataColor = colorObjectToColor(object.value(QStringLiteral("metadataColor")), render.metadataColor);

    if (object.value(QStringLiteral("titleScale")).isDouble())
        render.titleScale = qBound(1.0, object.value(QStringLiteral("titleScale")).toDouble(), 3.0);
    if (object.value(QStringLiteral("authorScale")).isDouble())
        render.authorScale = qBound(0.5, object.value(QStringLiteral("authorScale")).toDouble(), 1.5);
    if (object.value(QStringLiteral("dynastyScale")).isDouble())
        render.dynastyScale = qBound(0.5, object.value(QStringLiteral("dynastyScale")).toDouble(), 1.5);
    if (object.value(QStringLiteral("titleWeight")).isDouble())
        render.titleWeight = qBound(400, object.value(QStringLiteral("titleWeight")).toInt(), 900);
    if (object.value(QStringLiteral("contentLineSpacing")).isDouble())
        render.contentLineSpacing = qBound(1.0, object.value(QStringLiteral("contentLineSpacing")).toDouble(), 2.5);
    if (object.value(QStringLiteral("titleMetadataSpacing")).isDouble())
        render.titleMetadataSpacing = qBound(0, object.value(QStringLiteral("titleMetadataSpacing")).toInt(), 200);
    if (object.value(QStringLiteral("metadataContentSpacing")).isDouble())
        render.metadataContentSpacing = qBound(0, object.value(QStringLiteral("metadataContentSpacing")).toInt(), 300);
    if (object.value(QStringLiteral("metadataInlineSpacing")).isDouble())
        render.metadataInlineSpacing = qBound(0, object.value(QStringLiteral("metadataInlineSpacing")).toInt(), 100);

    if (object.value(QStringLiteral("panelEnabled")).isBool())
        render.panelEnabled = object.value(QStringLiteral("panelEnabled")).toBool();
    render.panelColor = colorObjectToColor(object.value(QStringLiteral("panelColor")), render.panelColor);
    if (object.value(QStringLiteral("shadowEnabled")).isBool())
        render.shadowEnabled = object.value(QStringLiteral("shadowEnabled")).toBool();
    if (object.value(QStringLiteral("fitMode")).isString())
        render.fitMode = fitModeFromString(object.value(QStringLiteral("fitMode")).toString());
    render.canvasBackgroundColor = colorObjectToColor(object.value(QStringLiteral("canvasBackgroundColor")), render.canvasBackgroundColor);
}

QJsonObject runtimeToJson(const RuntimeState &runtime) {
    return QJsonObject{
        {QStringLiteral("lastSuccessfulWallpaper"), runtime.lastSuccessfulWallpaper},
        {QStringLiteral("lastPoemId"), runtime.lastPoemId},
        {QStringLiteral("lastImagePath"), runtime.lastImagePath},
        {QStringLiteral("lastSuccessTime"), runtime.lastSuccessTime.toString(Qt::ISODate)},
        {QStringLiteral("lastMessage"), runtime.lastMessage},
        {QStringLiteral("currentPreviewWallpaper"), runtime.currentPreviewWallpaper},
        {QStringLiteral("currentPreviewPoemId"), runtime.currentPreviewPoemId},
        {QStringLiteral("currentPreviewImagePath"), runtime.currentPreviewImagePath},
        {QStringLiteral("currentPreviewApplied"), runtime.currentPreviewApplied}
    };
}

void runtimeFromJson(const QJsonObject &object, RuntimeState &runtime) {
    if (object.value(QStringLiteral("lastSuccessfulWallpaper")).isString())
        runtime.lastSuccessfulWallpaper = object.value(QStringLiteral("lastSuccessfulWallpaper")).toString();
    if (object.value(QStringLiteral("lastPoemId")).isString())
        runtime.lastPoemId = object.value(QStringLiteral("lastPoemId")).toString();
    if (object.value(QStringLiteral("lastImagePath")).isString())
        runtime.lastImagePath = object.value(QStringLiteral("lastImagePath")).toString();
    if (object.value(QStringLiteral("lastSuccessTime")).isString())
        runtime.lastSuccessTime = QDateTime::fromString(object.value(QStringLiteral("lastSuccessTime")).toString(), Qt::ISODate);
    if (object.value(QStringLiteral("lastMessage")).isString())
        runtime.lastMessage = object.value(QStringLiteral("lastMessage")).toString();
    if (object.value(QStringLiteral("currentPreviewWallpaper")).isString())
        runtime.currentPreviewWallpaper = object.value(QStringLiteral("currentPreviewWallpaper")).toString();
    if (object.value(QStringLiteral("currentPreviewPoemId")).isString())
        runtime.currentPreviewPoemId = object.value(QStringLiteral("currentPreviewPoemId")).toString();
    if (object.value(QStringLiteral("currentPreviewImagePath")).isString())
        runtime.currentPreviewImagePath = object.value(QStringLiteral("currentPreviewImagePath")).toString();
    if (object.value(QStringLiteral("currentPreviewApplied")).isBool())
        runtime.currentPreviewApplied = object.value(QStringLiteral("currentPreviewApplied")).toBool();
}
QJsonObject configToJson(const AppConfig &config) {
    QJsonArray sources;
    for (const ImageSource &source : config.imageSources) {
        sources.append(QJsonObject{
            {QStringLiteral("path"), source.path},
            {QStringLiteral("enabled"), source.enabled},
            {QStringLiteral("recursive"), source.recursive},
            {QStringLiteral("isFile"), source.isFile}
        });
    }

    QJsonArray poems;
    for (const Poem &poem : config.poems) poems.append(poemToJson(poem));

    return QJsonObject{
        {QStringLiteral("schemaVersion"), config.schemaVersion},
        {QStringLiteral("imageSources"), sources},
        {QStringLiteral("poetryLibrary"), QJsonObject{
            {QStringLiteral("path"), config.poetryLibraryPath},
            {QStringLiteral("embeddedEntries"), poems}
        }},
        {QStringLiteral("schedule"), QJsonObject{
            {QStringLiteral("enabled"), config.scheduleEnabled},
            {QStringLiteral("intervalMinutes"), config.intervalMinutes}
        }},
        {QStringLiteral("render"), renderToJson(config.render)},
        {QStringLiteral("runtime"), runtimeToJson(config.runtime)}
    };
}

bool configFromJson(const QJsonObject &object, AppConfig &config, QStringList *warnings) {
    config = ConfigStore::defaultConfig();
    if (object.value(QStringLiteral("schemaVersion")).isDouble())
        config.schemaVersion = qMax(1, object.value(QStringLiteral("schemaVersion")).toInt());

    const QJsonValue sourceValue = object.value(QStringLiteral("imageSources"));
    if (sourceValue.isArray()) {
        for (const QJsonValue &value : sourceValue.toArray()) {
            if (!value.isObject() || !value.toObject().value(QStringLiteral("path")).isString()) {
                if (warnings) warnings->append(QStringLiteral("忽略无效图片目录配置"));
                continue;
            }
            ImageSource source;
            source.path = QDir::cleanPath(value.toObject().value(QStringLiteral("path")).toString());
            source.enabled = value.toObject().value(QStringLiteral("enabled")).toBool(true);
            source.recursive = value.toObject().value(QStringLiteral("recursive")).toBool(true);
            source.isFile = value.toObject().value(QStringLiteral("isFile")).toBool(false);
            if (!source.path.isEmpty()) config.imageSources.append(source);
        }
    }

    const QJsonObject library = object.value(QStringLiteral("poetryLibrary")).toObject();
    if (library.value(QStringLiteral("path")).isString())
        config.poetryLibraryPath = library.value(QStringLiteral("path")).toString();
    const QJsonValue poemValue = library.value(QStringLiteral("embeddedEntries"));
    if (poemValue.isArray()) {
        QSet<QString> ids;
        for (const QJsonValue &value : poemValue.toArray()) {
            if (!value.isObject()) continue;
            QString error;
            Poem poem = poemFromJson(value.toObject(), &error);
            if (poem.content.isEmpty()) {
                if (warnings) warnings->append(QStringLiteral("忽略无效诗词：%1").arg(error));
                continue;
            }
            if (poem.id.isEmpty()) poem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (ids.contains(poem.id)) continue;
            ids.insert(poem.id);
            config.poems.append(poem);
        }
    }

    const QJsonObject schedule = object.value(QStringLiteral("schedule")).toObject();
    if (schedule.value(QStringLiteral("enabled")).isBool()) config.scheduleEnabled = schedule.value(QStringLiteral("enabled")).toBool();
    if (schedule.value(QStringLiteral("intervalMinutes")).isDouble())
        config.intervalMinutes = qBound(1, schedule.value(QStringLiteral("intervalMinutes")).toInt(), 1440);

    renderFromJson(object.value(QStringLiteral("render")).toObject(), config.render);
    runtimeFromJson(object.value(QStringLiteral("runtime")).toObject(), config.runtime);
    return true;
}

} // namespace

ConfigStore::ConfigStore() {
    m_rootPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (m_rootPath.isEmpty()) m_rootPath = QDir::homePath() + QStringLiteral("/.poetry_desktop");
}

ConfigStore::ConfigStore(const QString &rootOverride) : m_rootPath(rootOverride) {}

QString ConfigStore::rootPath() const { return m_rootPath; }
QString ConfigStore::configPath() const { return QDir(m_rootPath).filePath(QStringLiteral("config.json")); }
QString ConfigStore::cachePath() const { return QDir(m_rootPath).filePath(QStringLiteral("cache")); }
QString ConfigStore::wallpaperCachePath() const { return QDir(cachePath()).filePath(QStringLiteral("wallpapers")); }
QString ConfigStore::logPath() const { return QDir(m_rootPath).filePath(QStringLiteral("logs")); }

AppConfig ConfigStore::defaultConfig() {
    AppConfig config;
    config.intervalMinutes = 30;
    return config;
}

bool ConfigStore::load(AppConfig &config, QStringList *warnings) const {
    QDir().mkpath(m_rootPath);
    QFile file(configPath());
    if (!file.exists()) {
        config = defaultConfig();
        return save(config, nullptr);
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (warnings) warnings->append(QStringLiteral("配置文件无法读取：%1").arg(file.errorString()));
        config = defaultConfig();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString backup = configPath() + QStringLiteral(".corrupt-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        file.close();
        QFile::rename(configPath(), backup);
        if (warnings) warnings->append(QStringLiteral("配置损坏，已备份为：%1").arg(backup));
        config = defaultConfig();
        save(config, nullptr);
        return false;
    }
    return configFromJson(document.object(), config, warnings);
}

bool ConfigStore::save(const AppConfig &config, QString *error) const {
    if (!QDir().mkpath(m_rootPath) || !QDir().mkpath(wallpaperCachePath()) || !QDir().mkpath(logPath())) {
        if (error) *error = QStringLiteral("无法创建应用数据目录");
        return false;
    }
    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QJsonDocument document(configToJson(config));
    file.write(document.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool ConfigStore::backupAndReset(QString *error) const {
    if (QFile::exists(configPath())) {
        const QString backup = configPath() + QStringLiteral(".backup-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (!QFile::copy(configPath(), backup)) {
            if (error) *error = QStringLiteral("无法备份当前配置");
            return false;
        }
    }
    return save(defaultConfig(), error);
}

} // namespace poetry
