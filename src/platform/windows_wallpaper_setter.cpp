#include "windows_wallpaper_setter.h"

#include <QFileInfo>

#include <cwchar>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace poetry {

#ifdef Q_OS_WIN
namespace {

// The composited image is already exactly the panel size, so "Fill" is the only
// style that shows it 1:1; any other style would crop or letterbox it.
void forceFillStyle() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    const auto write = [key](const wchar_t *name, const wchar_t *value) {
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value),
                       static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    };
    write(L"WallpaperStyle", L"10");
    write(L"TileWallpaper", L"0");
    RegCloseKey(key);
}

} // namespace
#endif

bool WindowsWallpaperSetter::setWallpaper(const QString &absolutePath, QString *error) {
    if (!QFileInfo::exists(absolutePath)) {
        if (error) *error = QStringLiteral("壁纸文件不存在");
        return false;
    }
#ifdef Q_OS_WIN
    const std::wstring nativePath = absolutePath.toStdWString();
    forceFillStyle();
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

QString currentDesktopWallpaperPath() {
#ifdef Q_OS_WIN
    std::wstring buffer(32768, L'\0');
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, static_cast<UINT>(buffer.size()), buffer.data(), 0)) return {};
    buffer.resize(std::wcslen(buffer.c_str()));
    return QString::fromStdWString(buffer);
#else
    return {};
#endif
}

} // namespace poetry
