#include "core/config_store.h"
#include "core/libraries.h"
#include "core/renderer.h"
#include "core/wallpaper_service.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using namespace poetry;

class FakeWallpaperSetter final : public IWallpaperSetter {
public:
    bool success = true;
    QString path;
    bool setWallpaper(const QString &absolutePath, QString *error) override {
        path = absolutePath;
        if (!success && error) *error = QStringLiteral("fake failure");
        return success;
    }
};

class CoreTests final : public QObject {
    Q_OBJECT
private slots:
    void poemImportAndValidation();
    void invalidImportDoesNotChangeLibrary();
    void randomPickerAvoidsConsecutive();
    void rendererCreatesTargetWithText();
    void fontSettingChangesRenderedOutput();
    void rendererUsesLayeredSectionColors();
    void rendererHandlesEmptySectionFields();
    void rendererHandlesResolutionsAndLongText();
    void rendererFallsBackForUnavailableChineseFont();
    void oversizedPoemReturnsError();
    void imageScanRecursesAndDeduplicates();
    void configRoundTripAndCorruptionRecovery();
    void wallpaperServiceKeepsFailureSafe();
    void cacheCleanupPreservesCurrent();
    void cacheCleanupProtectsActiveWallpaperAcrossPreviews();
};

void CoreTests::poemImportAndValidation() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath(QStringLiteral("poems.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"schemaVersion":1,"poems":[{"id":"ok","title":"标题","author":"作者","content":"第一行\n第二行","enabled":true},{"id":"bad","content":""},{"id":"wrong","content":12}]})");
    file.close();

    PoetryLibrary library = PoetryLibrary::fromPoems({});
    const ImportResult result = library.importJson(path);
    QVERIFY(result.success);
    QCOMPARE(result.imported, 1);
    QCOMPARE(result.skipped, 2);
    QCOMPARE(library.all().first().content, QStringLiteral("第一行\n第二行"));
}

void CoreTests::invalidImportDoesNotChangeLibrary() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath(QStringLiteral("broken.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{not-json");
    file.close();

    const QVector<Poem> original = PoetryLibrary::defaultPoems().mid(0, 1);
    PoetryLibrary library = PoetryLibrary::fromPoems(original);
    const ImportResult result = library.importJson(path, true);
    QVERIFY(!result.success);
    QCOMPARE(library.all().size(), original.size());
    QCOMPARE(library.all().first().id, original.first().id);
}
void CoreTests::randomPickerAvoidsConsecutive() {
    QVector<Poem> poems{
        {QStringLiteral("a"), {}, {}, {}, QStringLiteral("A"), {}, true},
        {QStringLiteral("b"), {}, {}, {}, QStringLiteral("B"), {}, true}
    };
    RandomPicker picker(1234);
    picker.setForcedIndices({0, 0, 1, 1});
    const int first = picker.pickPoem(poems);
    picker.rememberPoem(poems.at(first).id);
    const int second = picker.pickPoem(poems);
    QVERIFY(first != second);
    picker.rememberPoem(poems.at(second).id);
    const int third = picker.pickPoem(poems);
    QVERIFY(second != third);
}

void CoreTests::rendererCreatesTargetWithText() {
    QImage source(QSize(640, 480), QImage::Format_ARGB32_Premultiplied);
    source.fill(QColor(30, 90, 140));
    Poem poem{QStringLiteral("id"), QStringLiteral("测试标题"), QStringLiteral("测试作者"), QStringLiteral("唐"), QStringLiteral("第一行\n第二行"), {}, true};
    RenderSettings settings;
    settings.fontPointSize = 26;
    WallpaperRenderer renderer;
    QString warning;
    const QImage output = renderer.render(source, poem, settings, QSize(320, 200), &warning);
    QVERIFY(!output.isNull());
    QCOMPARE(output.size(), QSize(320, 200));
    int differentPixels = 0;
    for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
            if (output.pixelColor(x, y) != source.pixelColor(qMin(source.width() - 1, x * 2), qMin(source.height() - 1, y * 2))) ++differentPixels;
        }
    }
    QVERIFY(differentPixels > 100);
}

void CoreTests::rendererUsesLayeredSectionColors() {
    QImage source(QSize(900, 600), QImage::Format_ARGB32_Premultiplied);
    source.fill(QColor(20, 20, 20));
    Poem poem{QStringLiteral("layered"), QStringLiteral("Title"), QStringLiteral("Author"), QStringLiteral("Dynasty"), QStringLiteral("Line one\nLine two"), {}, true};
    RenderSettings settings;
    settings.panelEnabled = false;
    settings.shadowEnabled = false;
    settings.fontFamily = QStringLiteral("Arial");
    settings.fontPointSize = 30;
    settings.titleColor = QColor(255, 40, 40);
    settings.metadataColor = QColor(40, 255, 40);
    settings.contentColor = QColor(40, 120, 255);
    settings.textColor = settings.contentColor;
    WallpaperRenderer renderer;
    const QImage output = renderer.render(source, poem, settings, QSize(900, 600));
    QVERIFY(!output.isNull());

    int red = 0;
    int green = 0;
    int blue = 0;
    for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
            const QColor c = output.pixelColor(x, y);
            if (c.red() > 210 && c.green() < 100 && c.blue() < 100) ++red;
            if (c.green() > 180 && c.red() < 100 && c.blue() < 150) ++green;
            if (c.blue() > 170 && c.red() < 100 && c.green() < 180) ++blue;
        }
    }
    QVERIFY(red > 0);
    QVERIFY(green > 0);
    QVERIFY(blue > 0);
}

void CoreTests::rendererHandlesEmptySectionFields() {
    QImage source(QSize(640, 420), QImage::Format_RGB32);
    source.fill(QColor(80, 90, 100));
    WallpaperRenderer renderer;
    const QString content = QStringLiteral("Line one\nLine two");
    const QVector<Poem> poems{
        {QStringLiteral("all"), QStringLiteral("Title"), QStringLiteral("Author"), QStringLiteral("Dynasty"), content, {}, true},
        {QStringLiteral("no-title"), {}, QStringLiteral("Author"), QStringLiteral("Dynasty"), content, {}, true},
        {QStringLiteral("no-author"), QStringLiteral("Title"), {}, QStringLiteral("Dynasty"), content, {}, true},
        {QStringLiteral("no-dynasty"), QStringLiteral("Title"), QStringLiteral("Author"), {}, content, {}, true},
        {QStringLiteral("only-body"), {}, {}, {}, content, {}, true}
    };
    for (const Poem &poem : poems) {
        QString error;
        const QImage output = renderer.render(source, poem, RenderSettings{}, QSize(640, 420), &error);
        QVERIFY2(!output.isNull(), qPrintable(error));
    }
}

void CoreTests::rendererHandlesResolutionsAndLongText() {
    QImage source(QSize(1200, 800), QImage::Format_RGB32);
    source.fill(QColor(50, 60, 70));
    Poem poem{QStringLiteral("sizes"), QStringLiteral("A title"), QStringLiteral("Author"), QStringLiteral("Dynasty"),
              QStringLiteral("One line\nTwo lines\nThree lines"), {}, true};
    WallpaperRenderer renderer;
    const QVector<QSize> sizes{QSize(320, 200), QSize(640, 360), QSize(1920, 1080), QSize(1080, 1920)};
    for (const QSize &size : sizes) {
        QString error;
        const QImage output = renderer.render(source, poem, RenderSettings{}, size, &error);
        QVERIFY2(!output.isNull(), qPrintable(QStringLiteral("%1: %2").arg(size.width()).arg(error)));
        QCOMPARE(output.size(), size);
    }

    Poem tooLong = poem;
    tooLong.content = QString(20000, QChar(0x4e00));
    QString error;
    QVERIFY(renderer.render(source, tooLong, RenderSettings{}, QSize(640, 360), &error).isNull());
    QVERIFY(error.contains(QStringLiteral("\u8fc7\u957f")));
}

void CoreTests::rendererFallsBackForUnavailableChineseFont() {
    QImage source(QSize(640, 420), QImage::Format_RGB32);
    source.fill(QColor(30, 30, 30));
    Poem poem{QStringLiteral("fallback"), QStringLiteral("\u5c71\u5c45\u79cb\u669d"), QStringLiteral("\u738b\u7ef4"), QStringLiteral("\u5510"),
              QStringLiteral("\u7a7a\u5c71\u65b0\u96e8\u540e\n\u5929\u6c14\u665a\u6765\u79cb"), {}, true};
    RenderSettings settings;
    settings.fontFamily = QStringLiteral("__Missing_Font_For_Test__");
    WallpaperRenderer renderer;
    QString warning;
    const QImage output = renderer.render(source, poem, settings, QSize(640, 420), &warning);
    QVERIFY(!output.isNull());
    QVERIFY(warning.contains(QStringLiteral("\u56de\u9000")));
}

void CoreTests::fontSettingChangesRenderedOutput() {
    QImage source(QSize(1200, 700), QImage::Format_RGB32);
    source.fill(Qt::black);
    // Isolate body glyphs: no panel, no shadow and no title/metadata. This
    // catches the regression where only layout/panel dimensions changed while
    // QPainter kept drawing with its default font.
    Poem poem{QStringLiteral("font"), {}, {}, {}, QStringLiteral("Font size 2026"), {}, true};
    WallpaperRenderer renderer;
    RenderSettings small;
    small.fontFamily = QStringLiteral("Arial");
    small.fontPointSize = 16;
    small.panelEnabled = false;
    small.shadowEnabled = false;
    small.contentColor = Qt::white;
    small.textColor = Qt::white;
    small.anchor = Anchor::TopLeft;
    RenderSettings large = small;
    large.fontPointSize = 48;

    const QImage first = renderer.render(source, poem, small, source.size());
    const QImage second = renderer.render(source, poem, large, source.size());
    QVERIFY(!first.isNull());
    QVERIFY(!second.isNull());

    auto brightPixelCount = [](const QImage &image) {
        qsizetype count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                if (color.red() + color.green() + color.blue() > 90) ++count;
            }
        }
        return count;
    };

    const qsizetype smallPixels = brightPixelCount(first);
    const qsizetype largePixels = brightPixelCount(second);
    QVERIFY(smallPixels > 0);
    QVERIFY2(largePixels > smallPixels * 2,
             qPrintable(QStringLiteral("larger font did not enlarge glyphs: small=%1 large=%2")
                 .arg(smallPixels).arg(largePixels)));
}

void CoreTests::oversizedPoemReturnsError() {
    QImage source(QSize(320, 200), QImage::Format_RGB32);
    source.fill(Qt::blue);
    Poem poem{QStringLiteral("long"), QString(), QString(), QString(), QString(20000, QChar(0x4e00)), {}, true};
    WallpaperRenderer renderer;
    QString error;
    const QImage output = renderer.render(source, poem, RenderSettings{}, QSize(320, 200), &error);
    QVERIFY(output.isNull());
    QVERIFY(error.contains(QStringLiteral("过长")));
}
void CoreTests::imageScanRecursesAndDeduplicates() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString nested = QDir(temp.path()).filePath(QStringLiteral("nested"));
    QVERIFY(QDir().mkpath(nested));
    QImage image(QSize(20, 20), QImage::Format_RGB32);
    image.fill(Qt::red);
    const QString imagePath = QDir(temp.path()).filePath(QStringLiteral("a.png"));
    QVERIFY(image.save(imagePath));
    QVERIFY(image.save(QDir(nested).filePath(QStringLiteral("b.png")), "PNG"));
    QFile(QDir(nested).filePath(QStringLiteral("bad.png"))).open(QIODevice::WriteOnly);
    const auto result = ImageLibrary::scan({ImageSource{temp.path(), true, true}, ImageSource{temp.path(), true, true}});
    QCOMPARE(result.items.size(), 2);
    QVERIFY(result.skipped >= 1);
}

void CoreTests::configRoundTripAndCorruptionRecovery() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    ConfigStore store(temp.path());
    AppConfig config = ConfigStore::defaultConfig();
    config.imageSources.append(ImageSource{QStringLiteral("C:/中文 图片"), false, true});
    config.poems = PoetryLibrary::defaultPoems();
    config.render.fontFamily = QStringLiteral("Arial");
    config.render.titleScale = 1.62;
    config.render.authorScale = 0.70;
    config.render.dynastyScale = 0.66;
    config.render.titleWeight = 700;
    config.render.titleColor = QColor(255, 220, 180);
    config.render.metadataColor = QColor(190, 190, 190, 220);
    config.render.contentColor = QColor(240, 240, 220);
    config.render.contentLineSpacing = 1.48;
    config.render.titleMetadataSpacing = 15;
    config.render.metadataContentSpacing = 28;
    config.render.metadataInlineSpacing = 11;
    config.intervalMinutes = 1440;
    config.runtime.currentPreviewWallpaper = QStringLiteral("C:/cache/preview.png");
    config.runtime.currentPreviewPoemId = QStringLiteral("default-001");
    config.runtime.currentPreviewImagePath = QStringLiteral("C:/images/a.png");
    config.runtime.currentPreviewApplied = false;
    QString error;
    QVERIFY(store.save(config, &error));

    AppConfig loaded;
    QVERIFY(store.load(loaded));
    QCOMPARE(loaded.imageSources.first().path, QStringLiteral("C:/中文 图片"));
    QVERIFY(!loaded.imageSources.first().enabled);
    QCOMPARE(loaded.poems.size(), 3);
    QCOMPARE(loaded.intervalMinutes, 1440);
    QCOMPARE(loaded.render.titleScale, 1.62);
    QCOMPARE(loaded.render.authorScale, 0.70);
    QCOMPARE(loaded.render.dynastyScale, 0.66);
    QCOMPARE(loaded.render.titleWeight, 700);
    QCOMPARE(loaded.render.titleColor, QColor(255, 220, 180));
    QCOMPARE(loaded.render.metadataColor, QColor(190, 190, 190, 220));
    QCOMPARE(loaded.render.contentColor, QColor(240, 240, 220));
    QCOMPARE(loaded.render.contentLineSpacing, 1.48);
    QCOMPARE(loaded.render.titleMetadataSpacing, 15);
    QCOMPARE(loaded.render.metadataContentSpacing, 28);
    QCOMPARE(loaded.render.metadataInlineSpacing, 11);
    QCOMPARE(loaded.runtime.currentPreviewWallpaper, QStringLiteral("C:/cache/preview.png"));
    QCOMPARE(loaded.runtime.currentPreviewPoemId, QStringLiteral("default-001"));
    QCOMPARE(loaded.runtime.currentPreviewImagePath, QStringLiteral("C:/images/a.png"));
    QVERIFY(!loaded.runtime.currentPreviewApplied);

    QFile corrupted(store.configPath());
    QVERIFY(corrupted.open(QIODevice::WriteOnly | QIODevice::Truncate));
    corrupted.write("{broken");
    corrupted.close();
    QStringList warnings;
    QVERIFY(!store.load(loaded, &warnings));
    QVERIFY(!warnings.isEmpty());
    QVERIFY(QFileInfo::exists(store.configPath()));

    AppConfig emptyPoems = ConfigStore::defaultConfig();
    QVERIFY(store.save(emptyPoems, &error));
    AppConfig emptyLoaded;
    QVERIFY(store.load(emptyLoaded));
    QCOMPARE(emptyLoaded.poems.size(), 0);
}

void CoreTests::wallpaperServiceKeepsFailureSafe() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString sourcePath = QDir(temp.path()).filePath(QStringLiteral("source.png"));
    QImage source(QSize(100, 100), QImage::Format_RGB32);
    source.fill(Qt::green);
    QVERIFY(source.save(sourcePath));
    QVector<ImageItem> images{ImageItem{sourcePath, QFileInfo(sourcePath).size(), QFileInfo(sourcePath).lastModified(), true, {}}};
    const QVector<Poem> poems = PoetryLibrary::defaultPoems().mid(0, 1);
    FakeWallpaperSetter setter;
    const SwitchResult success = WallpaperService::run(poems, images, RenderSettings{}, QSize(640, 480), temp.path(), RandomPicker(1), setter);
    QVERIFY2(success.success, qPrintable(success.message));
    QVERIFY(QFileInfo::exists(success.outputPath));
    QCOMPARE(setter.path, success.outputPath);

    FakeWallpaperSetter failedSetter;
    failedSetter.success = false;
    const SwitchResult failure = WallpaperService::run(poems, images, RenderSettings{}, QSize(640, 480), temp.path(), RandomPicker(1), failedSetter);
    QVERIFY(!failure.success);
    QVERIFY(failure.message.contains(QStringLiteral("fake failure")));
}

void CoreTests::cacheCleanupPreservesCurrent() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString current = QDir(temp.path()).filePath(QStringLiteral("wallpaper-current.png"));
    const QString old = QDir(temp.path()).filePath(QStringLiteral("wallpaper-old.png"));
    QImage image(QSize(10, 10), QImage::Format_RGB32);
    image.fill(Qt::black);
    QVERIFY(image.save(current, "PNG"));
    QVERIFY(image.save(old, "PNG"));
    WallpaperService::cleanupCache(temp.path(), current, 0);
    QVERIFY(QFileInfo::exists(current));
    QVERIFY(!QFileInfo::exists(old));
}

void CoreTests::cacheCleanupProtectsActiveWallpaperAcrossPreviews() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString sourcePath = QDir(temp.path()).filePath(QStringLiteral("source.png"));
    QImage source(QSize(160, 100), QImage::Format_RGB32);
    source.fill(QColor(70, 90, 110));
    QVERIFY(source.save(sourcePath, "PNG"));

    const ImageItem image{sourcePath, QFileInfo(sourcePath).size(), QFileInfo(sourcePath).lastModified(), true, {}};
    const Poem poem{QStringLiteral("cache"), QStringLiteral("??"), QStringLiteral("??"), QStringLiteral("?"), QStringLiteral("????"), {}, true};
    const RenderSettings settings;
    const QSize target(320, 200);

    const SwitchResult active = WallpaperService::renderPreview(poem, image, settings, target, temp.path());
    QVERIFY2(active.success, qPrintable(active.message));
    QVERIFY(QFileInfo::exists(active.outputPath));

    SwitchResult latest;
    for (int i = 0; i < 30; ++i) {
        latest = WallpaperService::renderPreview(poem, image, settings, target, temp.path());
        QVERIFY2(latest.success, qPrintable(latest.message));
    }
    QVERIFY(QFileInfo::exists(active.outputPath));
    QVERIFY(QFileInfo::exists(latest.outputPath));

    WallpaperService::cleanupCache(temp.path(), QStringList{active.outputPath, latest.outputPath}, 0);
    QVERIFY2(QFileInfo::exists(active.outputPath), "active desktop wallpaper was deleted by preview cleanup");
    QVERIFY(QFileInfo::exists(latest.outputPath));
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    CoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_core.moc"







