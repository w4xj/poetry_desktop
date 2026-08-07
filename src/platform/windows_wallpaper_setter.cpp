#include "windows_wallpaper_setter.h"

#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace poetry {

bool WindowsWallpaperSetter::setWallpaper(const QString &absolutePath, QString *error) {
    if (!QFileInfo::exists(absolutePath)) {
        if (error) *error = QStringLiteral("壁纸文件不存在");
        return false;
    }
#ifdef Q_OS_WIN
    const std::wstring nativePath = absolutePath.toStdWString();
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, const_cast<wchar_t *>(nativePath.c_str()),
                               SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
        const DWORD code = GetLastError();
        if (error) *error = QStringLiteral("SystemParametersInfoW 错误码 %1").arg(code);
        return false;
    }
    return true;
#else
    Q_UNUSED(absolutePath)
    if (error) *error = QStringLiteral("当前平台没有 Windows 壁纸实现");
    return false;
#endif
}

} // namespace poetry
