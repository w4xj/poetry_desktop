#include "windows_autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace poetry {

namespace {
constexpr auto kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto kValueName = "PoetryDesktop";
}

QString WindowsAutoStart::commandLine(const QString &executablePath) {
    if (executablePath.trimmed().isEmpty()) return {};
    return QStringLiteral("\"%1\" --autostart").arg(QDir::toNativeSeparators(executablePath));
}

bool WindowsAutoStart::setEnabled(bool enabled, QString *error) {
    if (error) error->clear();
#ifdef Q_OS_WIN
    QSettings settings(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        const QString executable = QCoreApplication::applicationFilePath();
        if (executable.isEmpty() || !QFileInfo::exists(executable)) {
            if (error) *error = QStringLiteral("无法定位程序文件");
            return false;
        }
        settings.setValue(QString::fromLatin1(kValueName), commandLine(executable));
    } else {
        settings.remove(QString::fromLatin1(kValueName));
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) *error = QStringLiteral("无法写入当前用户的启动项");
        return false;
    }
    return true;
#else
    Q_UNUSED(enabled)
    if (error) *error = QStringLiteral("当前平台不支持开机自启");
    return false;
#endif
}

} // namespace poetry
