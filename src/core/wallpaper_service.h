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

    // Takes the picker by reference so drawing advances the caller's state.
    // A by-value picker inside a worker thread left the original frozen on its
    // seed, and every call replayed the same draw.
    static SwitchResult run(const QVector<Poem> &poems,
                            const QVector<ImageItem> &images,
                            const RenderSettings &settings,
                            const QSize &targetSize,
                            const QString &cacheDirectory,
                            RandomPicker &picker,
                            IWallpaperSetter &setter,
                            const QStringList &protectedPaths = {});

    // Renders and applies one already chosen pair, so the caller can pick on its
    // own thread before handing the expensive work to a worker.
    static SwitchResult renderAndApply(const Poem &poem,
                                       const ImageItem &image,
                                       const RenderSettings &settings,
                                       const QSize &targetSize,
                                       const QString &cacheDirectory,
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

    // Saves the rendered wallpaper to a user-chosen path. The suffix decides the
    // format (PNG or JPG), so an exported file is always what its name says.
    static bool exportImage(const QString &sourcePath,
                            const QString &targetPath,
                            QString *error = nullptr);
};

} // namespace poetry
