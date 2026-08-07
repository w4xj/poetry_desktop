#pragma once

#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QSize>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace poetry {

enum class FitMode { Fill, Fit, Stretch };
enum class Anchor { TopLeft, TopRight, Center, BottomLeft, BottomRight };

QString fitModeToString(FitMode mode);
FitMode fitModeFromString(const QString &value);
QString anchorToString(Anchor anchor);
Anchor anchorFromString(const QString &value);

struct Poem {
    QString id;
    QString title;
    QString author;
    QString dynasty;
    QString content;
    QStringList tags;
    bool enabled = true;
};

struct ImageSource {
    QString path;
    bool enabled = true;
    bool recursive = true;
    // A source may be either a directory or one explicitly selected image.
    // The field is optional in persisted JSON for backwards compatibility.
    bool isFile = false;
};

struct ImageItem {
    QString path;
    qint64 size = 0;
    QDateTime lastModified;
    bool valid = false;
    QString error;
};

struct RenderSettings {
    FitMode fitMode = FitMode::Fill;
    Anchor anchor = Anchor::BottomRight;
    QString fontFamily = QStringLiteral("Microsoft YaHei UI");
    int fontPointSize = 36;

    // Legacy field retained for compatibility with older config files. It is
    // treated as the body/content color when contentColor is not present.
    QColor textColor = QColor(245, 241, 232, 255);

    // Layered typography settings. Ratios are relative to fontPointSize.
    double titleScale = 1.50;
    double authorScale = 0.72;
    double dynastyScale = 0.68;
    int titleWeight = 600;
    QColor titleColor = QColor(255, 255, 255, 255);
    QColor metadataColor = QColor(205, 205, 205, 220);
    QColor contentColor = QColor(245, 241, 232, 255);
    double contentLineSpacing = 1.35;
    int titleMetadataSpacing = 12;
    int metadataContentSpacing = 24;
    int metadataInlineSpacing = 10;

    bool panelEnabled = true;
    QColor panelColor = QColor(0, 0, 0, 153);
    bool shadowEnabled = true;
    QColor canvasBackgroundColor = QColor(32, 32, 32, 255);
};

struct RuntimeState {
    QString lastSuccessfulWallpaper;
    QString lastPoemId;
    QString lastImagePath;
    QDateTime lastSuccessTime;
    QString lastMessage;
    QString currentPreviewWallpaper;
    QString currentPreviewPoemId;
    QString currentPreviewImagePath;
    bool currentPreviewApplied = false;
};

struct AppConfig {
    int schemaVersion = 1;
    QVector<ImageSource> imageSources;
    QString poetryLibraryPath;
    QVector<Poem> poems;
    bool scheduleEnabled = false;
    int intervalMinutes = 30;
    RenderSettings render;
    RuntimeState runtime;
};

struct SwitchResult {
    bool success = false;
    QString message;
    QString outputPath;
    QString poemId;
    QString imagePath;
    QSize outputSize;
    Poem poem;
    ImageItem image;
};

struct ImportResult {
    bool success = false;
    int imported = 0;
    int skipped = 0;
    QStringList errors;
};

QJsonObject poemToJson(const Poem &poem);
Poem poemFromJson(const QJsonObject &object, QString *error = nullptr);

} // namespace poetry





