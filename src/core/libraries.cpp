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
    result.append(Poem{QStringLiteral("default-004"), QStringLiteral("登鹳雀楼"), QStringLiteral("王之涣"), QStringLiteral("唐"), QStringLiteral("白日依山尽，黄河入海流。\n欲穷千里目，更上一层楼。"), {QStringLiteral("唐诗"), QStringLiteral("登临")}, true});
    result.append(Poem{QStringLiteral("default-005"), QStringLiteral("春晓"), QStringLiteral("孟浩然"), QStringLiteral("唐"), QStringLiteral("春眠不觉晓，处处闻啼鸟。\n夜来风雨声，花落知多少。"), {QStringLiteral("唐诗"), QStringLiteral("四季")}, true});
    result.append(Poem{QStringLiteral("default-006"), QStringLiteral("江雪"), QStringLiteral("柳宗元"), QStringLiteral("唐"), QStringLiteral("千山鸟飞绝，万径人踪灭。\n孤舟蓑笠翁，独钓寒江雪。"), {QStringLiteral("唐诗"), QStringLiteral("冬景")}, true});
    result.append(Poem{QStringLiteral("default-007"), QStringLiteral("相思"), QStringLiteral("王维"), QStringLiteral("唐"), QStringLiteral("红豆生南国，春来发几枝。\n愿君多采撷，此物最相思。"), {QStringLiteral("唐诗"), QStringLiteral("相思")}, true});

    Poem gift;
    gift.id = QStringLiteral("default-008");
    gift.title = QStringLiteral("醉赠刘二十八使君");
    gift.author = QStringLiteral("白居易");
    gift.dynasty = QStringLiteral("唐");
    gift.content = QStringLiteral("为我引杯添酒饮，与君把箸击盘歌。\n诗称国手徒为尔，命压人头不奈何。\n举眼风光长寂寞，满朝官职独蹉跎。\n亦知合被才名折，二十三年折太多。");
    gift.tags = {QStringLiteral("唐诗"), QStringLiteral("赠答")};
    gift.secondTitle = QStringLiteral("酬乐天扬州初逢席上见赠");
    gift.secondAuthor = QStringLiteral("刘禹锡");
    gift.secondDynasty = QStringLiteral("唐");
    gift.secondContent = QStringLiteral("巴山楚水凄凉地，二十三年弃置身。\n怀旧空吟闻笛赋，到乡翻似烂柯人。\n沉舟侧畔千帆过，病树前头万木春。\n今日听君歌一曲，暂凭杯酒长精神。");
    gift.relation = QStringLiteral("gift");
    result.append(gift);
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

QVector<ImageItem> ImageLibrary::availableItems(const QStringList &excludedPaths) const {
    QSet<QString> excluded;
    for (const QString &path : excludedPaths) {
        if (path.isEmpty()) continue;
        excluded.insert(QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toCaseFolded());
    }
    QVector<ImageItem> result;
    for (const ImageItem &item : m_items) {
        if (!item.valid) continue;
        const QString normalized = QDir::cleanPath(QFileInfo(item.path).absoluteFilePath()).toCaseFolded();
        if (!excluded.contains(normalized)) result.append(item);
    }
    return result;
}

ImageLibrary::ScanResult ImageLibrary::scan(const QVector<ImageSource> &sources) {
    ScanResult result;
    QSet<QString> seen;
    QSet<QString> reportedMissingPlugins;
    QSet<QString> formats;
    for (const QByteArray &format : QImageReader::supportedImageFormats()) formats.insert(QString::fromLatin1(format).toLower());
    const QSet<QString> required = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("webp")};

    // Header-only validation: a full read() here decoded every candidate image
    // just to prove it was readable, which stalled scans of large photo folders.
    const auto acceptImage = [&](const QFileInfo &info) {
        const QString suffix = info.suffix().toLower();
        if (!required.contains(suffix)) return;
        const QString path = QDir::cleanPath(info.absoluteFilePath());
        if (seen.contains(path)) return;
        seen.insert(path);
        if (!formats.contains(suffix)) {
            ++result.skipped;
            if (!reportedMissingPlugins.contains(suffix)) {
                reportedMissingPlugins.insert(suffix);
                result.errors.append(QStringLiteral("Qt 缺少 %1 解码插件，该类型文件已全部跳过").arg(suffix.toUpper()));
            }
            return;
        }
        QImageReader reader(path);
        reader.setAutoTransform(true);
        if (!reader.canRead() || !reader.size().isValid()) {
            ++result.skipped;
            result.errors.append(QStringLiteral("无法读取图片：%1").arg(path));
            return;
        }
        result.items.append(ImageItem{path, info.size(), info.lastModified(), true, QString()});
    };

    for (const ImageSource &source : sources) {
        if (!source.enabled) continue;
        QFileInfo sourceInfo(source.path);
        if (source.isFile) {
            if (!sourceInfo.exists() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
                ++result.skipped;
                result.errors.append(QStringLiteral("图片文件不可用：%1").arg(source.path));
                continue;
            }
            acceptImage(sourceInfo);
            continue;
        }
        if (!sourceInfo.exists() || !sourceInfo.isDir() || !sourceInfo.isReadable()) {
            ++result.skipped;
            result.errors.append(QStringLiteral("图片目录不可用：%1").arg(source.path));
            continue;
        }
        QDirIterator iterator(sourceInfo.absoluteFilePath(), QStringList(), QDir::Files | QDir::NoSymLinks,
                              source.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
        while (iterator.hasNext()) acceptImage(QFileInfo(iterator.next()));
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


