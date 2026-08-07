#include "wallpaper_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace poetry {

SwitchResult WallpaperService::renderPreview(const Poem &poem,
                                             const ImageItem &image,
                                             const RenderSettings &settings,
                                             const QSize &targetSize,
                                             const QString &cacheDirectory) {
    SwitchResult result;
    result.poem = poem;
    result.image = image;
    result.poemId = poem.id;
    result.imagePath = image.path;

    if (!poem.enabled || poem.content.trimmed().isEmpty()) {
        result.message = QStringLiteral("当前诗词不可用");
        return result;
    }
    if (!image.valid || image.path.isEmpty() || !QFileInfo::exists(image.path)) {
        result.message = QStringLiteral("当前图片不存在或不可读取");
        return result;
    }

    QImageReader reader(image.path);
    reader.setAutoTransform(true);
    const QImage source = reader.read();
    if (source.isNull()) {
        result.message = QStringLiteral("图片读取失败：%1").arg(reader.errorString());
        return result;
    }

    WallpaperRenderer renderer;
    QString renderWarning;
    const QImage rendered = renderer.render(source, poem, settings, targetSize, &renderWarning);
    if (rendered.isNull()) {
        result.message = renderWarning.isEmpty() ? QStringLiteral("壁纸合成失败") : renderWarning;
        return result;
    }

    if (!QDir().mkpath(cacheDirectory)) {
        result.message = QStringLiteral("无法创建壁纸缓存目录：%1").arg(cacheDirectory);
        return result;
    }
    const QString fileName = QStringLiteral("wallpaper-%1-%2.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")),
             QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    const QString outputPath = QDir(cacheDirectory).filePath(fileName);
    QSaveFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly) || !rendered.save(&output, "PNG") || !output.commit()) {
        result.message = QStringLiteral("壁纸文件写入失败：%1").arg(output.errorString());
        return result;
    }

    QImage verify;
    if (!verify.load(outputPath) || verify.size() != targetSize) {
        result.message = QStringLiteral("壁纸文件校验失败");
        return result;
    }

    result.success = true;
    result.message = renderWarning.isEmpty() ? QStringLiteral("预览生成成功") : renderWarning;
    result.outputPath = outputPath;
    result.outputSize = rendered.size();
    // Preview generation must never delete cache files. The preview may be the
    // wallpaper currently referenced by Windows, and only the successful
    // wallpaper-apply path knows when it is safe to prune old files.
    return result;
}

SwitchResult WallpaperService::run(const QVector<Poem> &poems,
                                   const QVector<ImageItem> &images,
                                   const RenderSettings &settings,
                                   const QSize &targetSize,
                                   const QString &cacheDirectory,
                                   RandomPicker picker,
                                   IWallpaperSetter &setter,
                                   const QStringList &protectedPaths) {
    SwitchResult result;
    QVector<Poem> availablePoems;
    for (const Poem &poem : poems) {
        if (poem.enabled && !poem.content.trimmed().isEmpty()) availablePoems.append(poem);
    }
    QVector<ImageItem> availableImages;
    for (const ImageItem &image : images) {
        if (image.valid && QFileInfo::exists(image.path)) availableImages.append(image);
    }

    if (availablePoems.isEmpty()) {
        result.message = QStringLiteral("没有可用诗词，请导入或启用诗词条目");
        return result;
    }
    if (availableImages.isEmpty()) {
        result.message = QStringLiteral("没有可用图片，请添加包含图片的目录并重新扫描");
        return result;
    }

    const int poemIndex = picker.pickPoem(availablePoems);
    const int imageIndex = picker.pickImage(availableImages);
    if (poemIndex < 0 || imageIndex < 0) {
        result.message = QStringLiteral("随机抽取失败");
        return result;
    }

    result = renderPreview(availablePoems.at(poemIndex), availableImages.at(imageIndex), settings, targetSize, cacheDirectory);
    if (!result.success) return result;

    QString setError;
    if (!setter.setWallpaper(result.outputPath, &setError)) {
        result.success = false;
        result.message = setError.isEmpty() ? QStringLiteral("Windows 壁纸设置失败，当前桌面未改变")
                                            : QStringLiteral("壁纸设置失败：%1").arg(setError);
        return result;
    }

    // Only prune after Windows accepted the new wallpaper. Protect the new
    // file, the previous applied wallpaper, and any live preview explicitly.
    QStringList pathsToProtect = protectedPaths;
    pathsToProtect.append(result.outputPath);
    cleanupCache(cacheDirectory, pathsToProtect, 10);

    result.success = true;
    result.message = QStringLiteral("已应用到桌面：%1 / %2")
        .arg(result.poem.title.isEmpty() ? result.poem.content.left(12) : result.poem.title, result.image.path);
    return result;
}

SwitchResult WallpaperService::applyExisting(const QString &path,
                                             const Poem &poem,
                                             const ImageItem &image,
                                             IWallpaperSetter &setter,
                                             const QString &cacheDirectory,
                                             const QStringList &protectedPaths) {
    SwitchResult result;
    result.outputPath = path;
    result.poem = poem;
    result.image = image;
    result.poemId = poem.id;
    result.imagePath = image.path;
    if (!QFileInfo::exists(path)) {
        result.message = QStringLiteral("当前预览文件不存在，请重新预览");
        return result;
    }
    QString error;
    if (!setter.setWallpaper(path, &error)) {
        result.message = error.isEmpty() ? QStringLiteral("设置桌面失败，当前桌面未改变") : QStringLiteral("设置桌面失败：%1").arg(error);
        return result;
    }
    result.success = true;
    result.message = QStringLiteral("\u5df2\u5e94\u7528\u5230\u684c\u9762");
    if (!cacheDirectory.isEmpty()) {
        QStringList pathsToProtect = protectedPaths;
        pathsToProtect.append(path);
        cleanupCache(cacheDirectory, pathsToProtect, 10);
    }
    return result;
}

namespace {
QString normalizedCachePath(const QString &path) {
    if (path.isEmpty()) return {};
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toCaseFolded();
}
}

void WallpaperService::cleanupCache(const QString &cacheDirectory,
                                     const QStringList &protectedPaths,
                                     int keepCount) {
    QDir directory(cacheDirectory);
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("wallpaper-*.jpg"), QStringLiteral("wallpaper-*.png")},
                                                        QDir::Files, QDir::Time);
    QSet<QString> protectedSet;
    for (const QString &path : protectedPaths) {
        const QString normalized = normalizedCachePath(path);
        if (!normalized.isEmpty()) protectedSet.insert(normalized);
    }

    const int minimumKeepCount = qMax(0, keepCount);
    int kept = 0;
    for (const QFileInfo &file : files) {
        const QString normalized = normalizedCachePath(file.absoluteFilePath());
        if (protectedSet.contains(normalized) || kept < minimumKeepCount) {
            if (!protectedSet.contains(normalized)) ++kept;
            continue;
        }
        QFile::remove(file.absoluteFilePath());
    }
}

void WallpaperService::cleanupCache(const QString &cacheDirectory,
                                     const QString &currentPath,
                                     int keepCount) {
    cleanupCache(cacheDirectory, QStringList{currentPath}, keepCount);
}

} // namespace poetry
