#pragma once

#include "../core/wallpaper_service.h"

namespace poetry {

class WindowsWallpaperSetter final : public IWallpaperSetter {
public:
    bool setWallpaper(const QString &absolutePath, QString *error = nullptr) override;
};

} // namespace poetry
