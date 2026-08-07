#include "src/ui/main_window.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QLockFile>
#include <QStandardPaths>

using namespace poetry;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    app.setApplicationName(QStringLiteral("PoetryDesktop"));
    app.setApplicationDisplayName(QStringLiteral("诗词壁纸"));
    app.setOrganizationName(QStringLiteral("PoetryDesktop"));
    app.setOrganizationDomain(QStringLiteral("local.poetry.desktop"));

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appData.isEmpty()) appData = QDir::homePath() + QStringLiteral("/.poetry_desktop");
    QDir().mkpath(appData);
    QLockFile instanceLock(QDir(appData).filePath(QStringLiteral("poetry_desktop.lock")));
    instanceLock.setStaleLockTime(30000);
    if (!instanceLock.tryLock(100)) {
        QMessageBox::information(nullptr, QStringLiteral("诗词壁纸"), QStringLiteral("诗词壁纸已经在运行中。"));
        return 0;
    }

    MainWindow window;
    QObject::connect(&app, &QApplication::aboutToQuit, &window, &MainWindow::allowQuit);
    window.show();
    return app.exec();
}

