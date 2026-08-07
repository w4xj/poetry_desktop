#include "ui/main_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QtTest>

using namespace poetry;

class UiTests final : public QObject {
    Q_OBJECT
private slots:
    void mainWindowExposesReadablePrimaryActions();
    void poemManagementActionsArePresent();
    void scheduleControlsExposeSafeBounds();
};

void UiTests::mainWindowExposesReadablePrimaryActions() {
    MainWindow window;
    QVERIFY(window.minimumSize().width() >= 980);
    QVERIFY(window.minimumSize().height() >= 680);

    auto *switchButton = window.findChild<QPushButton *>(QStringLiteral("switchWallpaperButton"));
    auto *applyButton = window.findChild<QPushButton *>(QStringLiteral("applyPreviewButton"));
    auto *previewButton = window.findChild<QPushButton *>(QStringLiteral("refreshPreviewButton"));
    auto *fontSize = window.findChild<QSpinBox *>(QStringLiteral("fontSizeSpinBox"));
    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("resourceSettingsTabs"));

    QVERIFY(switchButton);
    QVERIFY(applyButton);
    QVERIFY(previewButton);
    QVERIFY(fontSize);
    QVERIFY(tabs);
    QVERIFY(switchButton->minimumHeight() >= 44);
    QVERIFY(switchButton->font().pointSize() >= 12);
    QVERIFY(applyButton->font().pointSize() >= 11);
    QVERIFY(fontSize->minimum() <= 12);
    QVERIFY(fontSize->maximum() >= 100);
    QVERIFY(tabs->count() >= 4);
}

void UiTests::poemManagementActionsArePresent() {
    MainWindow window;
    auto *addButton = window.findChild<QPushButton *>(QStringLiteral("addPoemButton"));
    auto *editButton = window.findChild<QPushButton *>(QStringLiteral("editPoemButton"));
    auto *importButton = window.findChild<QPushButton *>(QStringLiteral("importPoetryButton"));
    auto *deleteButton = window.findChild<QPushButton *>(QStringLiteral("deletePoemButton"));
    auto *useButton = window.findChild<QPushButton *>(QStringLiteral("useSelectedPoemPreviewButton"));
    QVERIFY(addButton);
    QVERIFY(editButton);
    QVERIFY(importButton);
    QVERIFY(deleteButton);
    QVERIFY(useButton);
    QVERIFY(!addButton->text().isEmpty());
    QVERIFY(!editButton->text().isEmpty());
}

void UiTests::scheduleControlsExposeSafeBounds() {
    MainWindow window;
    auto *enabled = window.findChild<QCheckBox *>(QStringLiteral("scheduleCheckBox"));
    auto *interval = window.findChild<QSpinBox *>(QStringLiteral("intervalSpinBox"));
    auto *next = window.findChild<QLabel *>(QStringLiteral("scheduleNextLabel"));
    QVERIFY(enabled);
    QVERIFY(interval);
    QVERIFY(next);
    QCOMPARE(interval->minimum(), 1);
    QCOMPARE(interval->maximum(), 1440);
    interval->setValue(1);
    QCOMPARE(interval->value(), 1);
    interval->setValue(1440);
    QCOMPARE(interval->value(), 1440);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_ui.moc"
