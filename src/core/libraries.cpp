#include "libraries.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QImageReader>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <random>
#include <utility>

namespace poetry {

PoetryLibrary PoetryLibrary::fromPoems(const QVector<Poem> &poems) {
    PoetryLibrary library;
    library.m_poems = poems;
    return library;
}

QVector<Poem> PoetryLibrary::enabledPoems() const {
    QVector<Poem> result;
    for (const Poem &poem : m_poems) {
        if (poem.enabled && !poem.content.trimmed().isEmpty()) result.append(poem);
    }
    return result;
}

QVector<Poem> PoetryLibrary::defaultPoems() {
    QVector<Poem> result;
    result.append(Poem{QStringLiteral("default-001"), QStringLiteral("静夜思"), QStringLiteral("李白"), QStringLiteral("唐"), QStringLiteral("床前明月光\n疑是地上霜\n举头望明月\n低头思故乡"), {QStringLiteral("唐诗"), QStringLiteral("思乡")}, true});
    result.append(Poem{QStringLiteral("default-002"), QStringLiteral("山居秋暝"), QStringLiteral("王维"), QStringLiteral("唐"), QStringLiteral("空山新雨后，天气晚来秋。\n明月松间照，清泉石上流。"), {QStringLiteral("山水"), QStringLiteral("唐诗")}, true});
    result.append(Poem{QStringLiteral("default-003"), QStringLiteral("饮酒·其五"), QStringLiteral("陶渊明"), QStringLiteral("晋"), QStringLiteral("采菊东篱下，悠然见南山。\n山气日夕佳，飞鸟相与还。"), {QStringLiteral("闲适")}, true});
    return result;
}

ImportResult PoetryLibrary::importJson(const QString &filePath, bool merge) {
    ImportResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errors.append(QStringLiteral("无法打开文件：%1").arg(file.errorString()));
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.errors.append(QStringLiteral("JSON 格式错误：%1").arg(parseError.errorString()));
        return result;
    }
    const QJsonValue poemValue = document.object().value(QStringLiteral("poems"));
    if (!poemValue.isArray()) {
        result.errors.append(QStringLiteral("顶层必须包含 poems 数组"));
        return result;
    }
    result.success = true;

    if (!merge) m_poems.clear();
    QSet<QString> ids;
    for (const Poem &poem : std::as_const(m_poems)) ids.insert(poem.id);
    for (const QJsonValue &value : poemValue.toArray()) {
        if (!value.isObject()) {
            ++result.skipped;
            result.errors.append(QStringLiteral("跳过非对象诗词条目"));
            continue;
        }
        QString error;
        Poem poem = poemFromJson(value.toObject(), &error);
        if (poem.content.isEmpty()) {
            ++result.skipped;
            result.errors.append(error);
            continue;
        }
        if (poem.id.isEmpty()) poem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (ids.contains(poem.id)) {
            ++result.skipped;
            result.errors.append(QStringLiteral("跳过重复 ID：%1").arg(poem.id));
            continue;
        }
        ids.insert(poem.id);
        m_poems.append(poem);
        ++result.imported;
    }
    return result;
}

bool PoetryLibrary::setEnabled(const QString &id, bool enabled) {
    for (Poem &poem : m_poems) {
        if (poem.id == id) {
            poem.enabled = enabled;
            return true;
        }
    }
    return false;
}

bool PoetryLibrary::remove(const QString &id) {
    for (qsizetype i = 0; i < m_poems.size(); ++i) {
        if (m_poems.at(i).id == id) {
            m_poems.removeAt(i);
            return true;
        }
    }
    return false;
}

QVector<ImageItem> ImageLibrary::validItems() const {
    QVector<ImageItem> result;
    for (const ImageItem &item : m_items) if (item.valid) result.append(item);
    return result;
}

ImageLibrary::ScanResult ImageLibrary::scan(const QVector<ImageSource> &sources) {
    ScanResult result;
    QSet<QString> seen;
    QSet<QString> formats;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) formats.insert(QString::fromLatin1(format).toLower());
    const QSet<QString> required = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("webp")};

    for (const ImageSource &source : sources) {
        if (!source.enabled) continue;
        QFileInfo sourceInfo(source.path);
        if (source.isFile) {
            if (!sourceInfo.exists() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
                ++result.skipped;
                result.errors.append(QStringLiteral("\u56fe\u7247\u6587\u4ef6\u4e0d\u53ef\u7528\uff1a%1").arg(source.path));
                continue;
            }
            const QString path = QDir::cleanPath(sourceInfo.absoluteFilePath());
            const QString suffix = sourceInfo.suffix().toLower();
            if (!required.contains(suffix) || !formats.contains(suffix) || seen.contains(path)) continue;
            seen.insert(path);
            QImageReader reader(path);
            reader.setAutoTransform(true);
            if (!reader.canRead() || reader.read().isNull()) {
                ++result.skipped;
                result.errors.append(QStringLiteral("\u56fe\u7247\u89e3\u7801\u5931\u8d25\uff1a%1").arg(path));
                continue;
            }
            result.items.append(ImageItem{path, sourceInfo.size(), sourceInfo.lastModified(), true, QString()});
            continue;
        }
        if (!sourceInfo.exists() || !sourceInfo.isDir() || !sourceInfo.isReadable()) {
            ++result.skipped;
            result.errors.append(QStringLiteral("\u56fe\u7247\u6587\u4ef6\u4e0d\u53ef\u7528\uff1a%1").arg(source.path));
            continue;
        }
        QDirIterator iterator(sourceInfo.absoluteFilePath(), QStringList(), QDir::Files | QDir::NoSymLinks,
                              source.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
        while (iterator.hasNext()) {
            const QString path = QDir::cleanPath(QFileInfo(iterator.next()).absoluteFilePath());
            const QFileInfo info(path);
            const QString suffix = info.suffix().toLower();
            if (!required.contains(suffix) || !formats.contains(suffix)) continue;
            if (seen.contains(path)) continue;
            seen.insert(path);
            QImageReader reader(path);
            reader.setAutoTransform(true);
            if (!reader.canRead()) {
                ++result.skipped;
                result.errors.append(QStringLiteral("无法读取图片：%1").arg(path));
                continue;
            }
            const QImage image = reader.read();
            if (image.isNull()) {
                ++result.skipped;
                result.errors.append(QStringLiteral("图片解码失败：%1").arg(path));
                continue;
            }
            result.items.append(ImageItem{path, info.size(), info.lastModified(), true, QString()});
        }
    }
    return result;
}

RandomPicker::RandomPicker() : RandomPicker(static_cast<quint32>(std::random_device{}())) {}
RandomPicker::RandomPicker(quint32 seed) { m_generator.seed(seed); }

void RandomPicker::setSeed(quint32 seed) {
    m_generator.seed(seed);
    m_forcedPosition = 0;
}

void RandomPicker::setForcedIndices(const QVector<int> &indices) {
    m_forcedIndices = indices;
    m_forcedPosition = 0;
}

void RandomPicker::clearForcedIndices() {
    m_forcedIndices.clear();
    m_forcedPosition = 0;
}

int RandomPicker::pickIndex(int count, int lastIndex) {
    if (count <= 0) return -1;
    int candidate = 0;
    if (m_forcedPosition < m_forcedIndices.size()) {
        candidate = m_forcedIndices.at(m_forcedPosition++);
        candidate = qAbs(candidate) % count;
    } else {
        std::uniform_int_distribution<int> distribution(0, count - 1);
        candidate = distribution(m_generator);
    }
    if (count > 1 && candidate == lastIndex) candidate = (candidate + 1) % count;
    return candidate;
}

int RandomPicker::pickPoem(const QVector<Poem> &poems) {
    int last = -1;
    for (int i = 0; i < poems.size(); ++i) if (poems.at(i).id == m_lastPoemId) { last = i; break; }
    return pickIndex(poems.size(), last);
}

int RandomPicker::pickImage(const QVector<ImageItem> &items) {
    int last = -1;
    for (int i = 0; i < items.size(); ++i) if (items.at(i).path == m_lastImagePath) { last = i; break; }
    return pickIndex(items.size(), last);
}

} // namespace poetry


