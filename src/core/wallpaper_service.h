#pragma once

#include "libraries.h"
#include "models.h"
#include "renderer.h"

#include <QSize>
#include <QString>
#include <QStringList>

namespace poetry {

class IWallpaperSetter {
public:
    virtual ~IWallpaperSetter() = default;
    virtual bool setWallpaper(const QString &absolutePath, QString *error = nullptr) = 0;
};

class WallpaperService {
public:
    static SwitchResult renderPreview(const Poem &poem,
                                      const ImageItem &image,
                                      const RenderSettings &settings,
                                      const QSize &targetSize,
                                      const QString &cacheDirectory);

    static SwitchResult run(const QVector<Poem> &poems,
                            const QVector<ImageItem> &images,
                            const RenderSettings &settings,
                            const QSize &targetSize,
                            const QString &cacheDirectory,
                            RandomPicker picker,
                            IWallpaperSetter &setter,
                            const QStringList &protectedPaths = {});

    static SwitchResult applyExisting(const QString &path,
                                      const Poem &poem,
                                      const ImageItem &image,
                                      IWallpaperSetter &setter,
                                      const QString &cacheDirectory = {},
                                      const QStringList &protectedPaths = {});

    static void cleanupCache(const QString &cacheDirectory,
                             const QStringList &protectedPaths,
                             int keepCount = 10);
    static void cleanupCache(const QString &cacheDirectory,
                             const QString &currentPath,
                             int keepCount = 10);
};

} // namespace poetry
