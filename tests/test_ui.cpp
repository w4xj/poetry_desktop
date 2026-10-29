#include "ui/main_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QDir>
#include <QFile>
#include <QToolButton>
#include <QTimer>
#include <QWizard>
#include <QtTest>

using namespace poetry;

class UiTests final : public QObject {
    Q_OBJECT
private slots:
    void mainWindowExposesReadablePrimaryActions();
    void poemManagementActionsArePresent();
    void scheduleControlsExposeSafeBounds();
    void layoutKeepsEveryRegionVisibleAndThePreviewDominant();
    void settingsControlsNeverOverlapEachOther();
    void historyAndExportActionsArePresent();
    void onboardingAddsSampleImagesWithoutApplying();
};

// A QFormLayout squeezed for vertical height collapses its rows and paints the
// labels on top of each other, which no size check catches on its own. Walk each
// tab page and assert sibling controls keep disjoint rectangles.
void UiTests::settingsControlsNeverOverlapEachOther() {
    MainWindow window(nullptr, false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("resourceSettingsTabs"));
    QVERIFY(tabs);

    // Checked at the default size and at the smallest size the app allows,
    // because a squeezed QFormLayout is exactly what collapsed before.
    for (const QSize &canvas : {QSize(1440, 900), QSize(1280, 800), QSize(1024, 700), QSize(900, 640)}) {
        window.resize(canvas);
        QTest::qWait(60);

        // The action bar gained a fifth control in v0.2; at the minimum window
        // width it must still show the full label rather than clipping it.
        auto *exportButton = window.findChild<QToolButton *>(QStringLiteral("exportMenuButton"));
        QVERIFY(exportButton);
        QVERIFY(exportButton->isVisible());
        QVERIFY2(exportButton->width() >= exportButton->minimumSizeHint().width(),
                 qPrintable(QStringLiteral("at %1x%2 the export button is %3 px, needs %4")
                     .arg(canvas.width()).arg(canvas.height())
                     .arg(exportButton->width()).arg(exportButton->minimumSizeHint().width())));
        const QRect exportRect(exportButton->mapTo(&window, QPoint(0, 0)), exportButton->size());
        QVERIFY2(window.rect().contains(exportRect), "the export button escapes the window");

        for (int page = 0; page < tabs->count(); ++page) {
            tabs->setCurrentIndex(page);
            QTest::qWait(30);
            QWidget *current = tabs->currentWidget();
            QVERIFY(current);

            QList<QWidget *> controls;
            const QList<QWidget *> descendants = current->findChildren<QWidget *>();
            for (QWidget *widget : descendants) {
                const bool interesting = qobject_cast<QPushButton *>(widget)
                    || qobject_cast<QComboBox *>(widget)
                    || qobject_cast<QSpinBox *>(widget)
                    || qobject_cast<QCheckBox *>(widget)
                    || qobject_cast<QLabel *>(widget)
                    || qobject_cast<QListWidget *>(widget)
                    || qobject_cast<QTableWidget *>(widget);
                if (!interesting || !widget->isVisible() || widget->rect().isEmpty()) continue;
                controls.append(widget);
            }
            QVERIFY2(controls.size() > 1, qPrintable(QStringLiteral("tab %1 exposed no controls").arg(page)));

            const auto rectText = [](const QRect &r) {
                return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
            };
            for (int i = 0; i < controls.size(); ++i) {
                for (int j = i + 1; j < controls.size(); ++j) {
                    const QRect a(controls.at(i)->mapTo(&window, QPoint(0, 0)), controls.at(i)->size());
                    const QRect b(controls.at(j)->mapTo(&window, QPoint(0, 0)), controls.at(j)->size());
                    if (a.contains(b) || b.contains(a)) continue;   // nesting is fine
                    const QRect both = a.intersected(b);
                    QVERIFY2(both.width() <= 2 || both.height() <= 2,
                             qPrintable(QStringLiteral("at %1x%2, tab %3: %4 (%5) overlaps %6 (%7) in %8")
                                 .arg(canvas.width()).arg(canvas.height()).arg(page)
                                 .arg(QLatin1String(controls.at(i)->metaObject()->className()), rectText(a))
                                 .arg(QLatin1String(controls.at(j)->metaObject()->className()), rectText(b))
                                 .arg(rectText(both))));
                }
            }
        }
    }
    window.resize(1440, 900);
}

// Object-name presence alone did not catch a regression where a list control
// stopped painting entirely, so assert real geometry after a layout pass.
// QStackedWidget only lays out the page that is currently shown, so each tab
// has to be made current before its children have a meaningful geometry.
void UiTests::layoutKeepsEveryRegionVisibleAndThePreviewDominant() {
    MainWindow window(nullptr, false);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("resourceSettingsTabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 5);

    const QVector<QPair<const char *, QWidget *>> alwaysVisible = {
        {"previewLabel", window.findChild<QLabel *>(QStringLiteral("previewLabel"))},
        {"resourceSettingsTabs", tabs},
        {"switchWallpaperButton", window.findChild<QPushButton *>(QStringLiteral("switchWallpaperButton"))},
        {"applyPreviewButton", window.findChild<QPushButton *>(QStringLiteral("applyPreviewButton"))},
        {"statusLabel", window.findChild<QLabel *>(QStringLiteral("statusLabel"))},
        {"desktopStateLabel", window.findChild<QLabel *>(QStringLiteral("desktopStateLabel"))},
    };
    for (const auto &region : alwaysVisible) {
        QWidget *widget = region.second;
        const QString label = QString::fromLatin1(region.first);
        QVERIFY2(widget, qPrintable(label));
        const QRect inside(widget->mapTo(&window, QPoint(0, 0)), widget->size());
        QVERIFY2(widget->width() > 60 && widget->height() >= 12,
                 qPrintable(QStringLiteral("%1 collapsed to %2x%3")
                     .arg(label).arg(widget->width()).arg(widget->height())));
        QVERIFY2(window.rect().adjusted(-1, -1, 1, 1).contains(inside),
                 qPrintable(QStringLiteral("%1 escapes the window at %2,%3 %4x%5")
                     .arg(label).arg(inside.x()).arg(inside.y())
                     .arg(inside.width()).arg(inside.height())));
    }

    auto *preview = alwaysVisible.first().second;
    QVERIFY2(preview->width() >= window.width() * 2 / 5,
             qPrintable(QStringLiteral("preview width %1 is not dominant").arg(preview->width())));
    QVERIFY2(preview->height() >= window.height() * 45 / 100,
             qPrintable(QStringLiteral("preview height %1 is not dominant").arg(preview->height())));
    QVERIFY2(tabs->width() >= 320 && tabs->height() >= 220,
             qPrintable(QStringLiteral("settings panel is %1x%2").arg(tabs->width()).arg(tabs->height())));

    // Floors encode design intent: the two resource lists were once squeezed
    // until a single row was usable, which is exactly what these catch.
    struct PageFloor { int tab; const char *name; int width; int height; };
    const QVector<PageFloor> floors = {
        {0, "imageDirectoryList", 120, 70},
        {0, "imageFileList", 120, 90},
        {1, "poemTable", 260, 90},
        {2, "historyTable", 260, 90},
        {2, "historyFilterComboBox", 70, 24},
        {3, "fontComboBox", 120, 24},
        {3, "fontSizeSpinBox", 70, 24},
        {3, "titleColorButton", 60, 24},
        {4, "intervalSpinBox", 70, 24},
        {4, "scheduleNextLabel", 80, 12},
        {4, "autostartCheckBox", 120, 20},
    };
    for (const PageFloor &floor : floors) {
        tabs->setCurrentIndex(floor.tab);
        QTest::qWait(30);
        auto *widget = window.findChild<QWidget *>(QString::fromLatin1(floor.name));
        QVERIFY2(widget, floor.name);
        QVERIFY2(widget->width() >= floor.width && widget->height() >= floor.height,
                 qPrintable(QStringLiteral("%1 is %2x%3, expected at least %4x%5")
                     .arg(QString::fromLatin1(floor.name)).arg(widget->width()).arg(widget->height())
                     .arg(floor.width).arg(floor.height)));
    }
}

void UiTests::mainWindowExposesReadablePrimaryActions() {
    MainWindow window(nullptr, false);
    // Floor keeps the window usable; the real per-size layout guarantee lives in
    // settingsControlsNeverOverlapEachOther, which measures at each size.
    QVERIFY(window.minimumSize().width() >= 860);
    QVERIFY(window.minimumSize().height() >= 600);

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
    QVERIFY(window.findChild<QTableWidget *>(QStringLiteral("historyTable")));
    QVERIFY(window.findChild<QToolButton *>(QStringLiteral("exportMenuButton")));
    QVERIFY(window.findChild<QCheckBox *>(QStringLiteral("autostartCheckBox")));
}

void UiTests::historyAndExportActionsArePresent() {
    MainWindow window(nullptr, false);
    auto *historyTable = window.findChild<QTableWidget *>(QStringLiteral("historyTable"));
    auto *filter = window.findChild<QComboBox *>(QStringLiteral("historyFilterComboBox"));
    auto *apply = window.findChild<QPushButton *>(QStringLiteral("applyHistoryButton"));
    auto *favorite = window.findChild<QPushButton *>(QStringLiteral("favoriteHistoryButton"));
    auto *exclude = window.findChild<QPushButton *>(QStringLiteral("excludeHistoryImageButton"));
    auto *exportButton = window.findChild<QToolButton *>(QStringLiteral("exportMenuButton"));
    auto *excludeImage = window.findChild<QPushButton *>(QStringLiteral("excludeImageButton"));
    QVERIFY(historyTable);
    QVERIFY(filter);
    QVERIFY(apply);
    QVERIFY(favorite);
    QVERIFY(exclude);
    QVERIFY(exportButton);
    QVERIFY(excludeImage);
    QCOMPARE(historyTable->columnCount(), 4);
    QCOMPARE(filter->count(), 2);
    QVERIFY(filter->findData(QStringLiteral("favorites")) >= 0);
    QVERIFY(exportButton->menu());
    QCOMPARE(exportButton->menu()->actions().size(), 2);
    QVERIFY(!apply->isEnabled());
    QVERIFY(!favorite->isEnabled());
}

void UiTests::poemManagementActionsArePresent() {
    MainWindow window(nullptr, false);
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
    MainWindow window(nullptr, false);
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

// Drives the real wizard: accept the defaults (built-in samples) and confirm the
// three JPEGs are copied out of qrc and registered as an image source. The config
// already exists here, so this is not a first run and no wallpaper is applied.
void UiTests::onboardingAddsSampleImagesWithoutApplying() {
    ConfigStore store;
    AppConfig seed = ConfigStore::defaultConfig();
    seed.poems = PoetryLibrary::defaultPoems();
    QVERIFY(store.save(seed));

    MainWindow window(nullptr, false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    bool sawWizard = false;
    int pageCount = 0;
    bool advanced = false;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&] {
        auto *wizard = qobject_cast<QWizard *>(QApplication::activeModalWidget());
        if (!wizard) return;
        poll.stop();
        sawWizard = true;
        pageCount = wizard->pageIds().size();
        wizard->next();
        advanced = wizard->currentId() == 1;
        wizard->accept();
    });
    poll.start(10);
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, [] {
        if (QWidget *modal = QApplication::activeModalWidget()) modal->close();
    });
    watchdog.start(5000);

    QVERIFY(QMetaObject::invokeMethod(&window, "showOnboarding", Qt::DirectConnection));
    QVERIFY2(sawWizard, "onboarding wizard never appeared");
    QCOMPARE(pageCount, 2);
    QVERIFY2(advanced, "the wizard did not advance to the image page");

    AppConfig loaded;
    QVERIFY(store.load(loaded));
    bool hasSampleSource = false;
    for (const ImageSource &source : loaded.imageSources) {
        if (QDir::cleanPath(source.path).endsWith(QStringLiteral("/samples"))) hasSampleSource = true;
    }
    QVERIFY2(hasSampleSource, "the sample folder was not added to the image sources");
    QVERIFY2(loaded.history.isEmpty(), "onboarding applied a wallpaper instead of only adding images");
    const QString sampleDir = QDir(store.rootPath()).filePath(QStringLiteral("samples"));
    QCOMPARE(QDir(sampleDir).entryList({QStringLiteral("*.jpg")}, QDir::Files).size(), 3);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_ui.moc"
