#pragma once

#include "../core/wallpaper_service.h"

namespace poetry {

class WindowsWallpaperSetter final : public IWallpaperSetter {
public:
    bool setWallpaper(const QString &absolutePath, QString *error = nullptr) override;
};

// Returns the current desktop wallpaper path, or an empty string when Windows
// uses a solid colour or cannot report one.
QString currentDesktopWallpaperPath();

} // namespace poetry
