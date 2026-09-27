#include "renderer.h"

#include <QFontDatabase>
#include <QFontMetrics>
#include <QPainter>
#include <QStringList>

namespace poetry {

namespace {

// 避头尾：收尾标点不得成为行首，起首标点不得留在行尾。
bool startsLineForbidden(QChar ch) {
    return QStringLiteral("，。、；：！？）》」』】〉…—·%）]}\"'").contains(ch);
}

bool endsLineForbidden(QChar ch) {
    return QStringLiteral("（《「『【〈“‘([").contains(ch);
}

struct WrappedText {
    QStringList lines;
    QFont font;
    QColor color;
    int lineHeight = 0;
    int width = 0;
    int height = 0;

    bool isEmpty() const { return lines.isEmpty(); }
};

WrappedText makeWrapped(const QString &text, const QFont &font, const QColor &color,
                        int maxWidth, qreal lineSpacing) {
    WrappedText wrapped;
    wrapped.font = font;
    wrapped.color = color;
    const QFontMetrics metrics(font);
    wrapped.lines = wrapPoemText(text, metrics, maxWidth);
    wrapped.lineHeight = qMax(metrics.height(), qRound(metrics.height() * lineSpacing));
    for (const QString &line : wrapped.lines)
        wrapped.width = qMax(wrapped.width, metrics.horizontalAdvance(line));
    wrapped.height = wrapped.lines.size() * wrapped.lineHeight;
    return wrapped;
}

// One half of what gets rendered. A single poem yields one part; a 酬答/唱和/问答
// poem yields two, each carrying its own heading tag.
struct PoemPart {
    QString tag;
    QString title;
    QString author;
    QString dynasty;
    QString content;
};

struct BlockLayout {
    WrappedText tag;
    WrappedText title;
    WrappedText author;
    WrappedText dynasty;
    WrappedText separator;
    WrappedText content;
    bool metadataInline = false;
    int metadataWidth = 0;
    int metadataHeight = 0;
    int totalWidth = 0;
    int totalHeight = 0;
};

BlockLayout buildBlock(const PoemPart &part, const RenderSettings &settings,
                       const QString &family, int maxWidth, int basePointSize) {
    BlockLayout layout;
    const int titleSize = qBound(10, qRound(basePointSize * settings.titleScale), 180);
    const int authorSize = qBound(8, qRound(basePointSize * settings.authorScale), 120);
    const int dynastySize = qBound(8, qRound(basePointSize * settings.dynastyScale), 120);

    QFont titleFont(family);
    titleFont.setPointSize(titleSize);
    titleFont.setWeight(static_cast<QFont::Weight>(qBound(400, settings.titleWeight, 900)));
    titleFont.setStyleStrategy(QFont::PreferAntialias);

    QFont authorFont(family);
    authorFont.setPointSize(authorSize);
    authorFont.setWeight(QFont::Medium);
    authorFont.setStyleStrategy(QFont::PreferAntialias);

    QFont dynastyFont(family);
    dynastyFont.setPointSize(dynastySize);
    dynastyFont.setWeight(QFont::Normal);
    dynastyFont.setStyleStrategy(QFont::PreferAntialias);

    QFont contentFont(family);
    contentFont.setPointSize(basePointSize);
    contentFont.setWeight(QFont::Normal);
    contentFont.setStyleStrategy(QFont::PreferAntialias);

    if (!part.tag.trimmed().isEmpty())
        layout.tag = makeWrapped(part.tag.trimmed(), authorFont, settings.metadataColor, maxWidth, 1.10);
    if (!part.title.trimmed().isEmpty())
        layout.title = makeWrapped(part.title.trimmed(), titleFont, settings.titleColor, maxWidth, 1.10);
    if (!part.author.trimmed().isEmpty())
        layout.author = makeWrapped(part.author.trimmed(), authorFont, settings.metadataColor, maxWidth, 1.10);
    if (!part.dynasty.trimmed().isEmpty())
        layout.dynasty = makeWrapped(part.dynasty.trimmed(), dynastyFont, settings.dynastyColor, maxWidth, 1.10);
    layout.content = makeWrapped(part.content.trimmed(), contentFont,
                                 settings.contentColor.isValid() ? settings.contentColor : settings.textColor,
                                 maxWidth, qMax<qreal>(1.0, settings.contentLineSpacing));

    const bool hasAuthor = !layout.author.isEmpty();
    const bool hasDynasty = !layout.dynasty.isEmpty();
    WrappedText mark;
    if (hasAuthor && hasDynasty) {
        QFont markFont(family);
        markFont.setPointSize(dynastySize);
        markFont.setWeight(QFont::Normal);
        markFont.setStyleStrategy(QFont::PreferAntialias);
        mark = makeWrapped(QStringLiteral("·"), markFont, settings.metadataColor, maxWidth, 1.10);
    }
    if (hasAuthor && hasDynasty && layout.author.lines.size() == 1 && layout.dynasty.lines.size() == 1
        && layout.author.width + mark.width + 2 * settings.metadataInlineSpacing + layout.dynasty.width <= maxWidth) {
        layout.metadataInline = true;
        layout.separator = mark;
        layout.metadataWidth = layout.author.width + settings.metadataInlineSpacing + mark.width
            + settings.metadataInlineSpacing + layout.dynasty.width;
        layout.metadataHeight = qMax(layout.author.height, layout.dynasty.height);
    } else if (hasAuthor || hasDynasty) {
        layout.metadataWidth = qMax(hasAuthor ? layout.author.width : 0, hasDynasty ? layout.dynasty.width : 0);
        layout.metadataHeight = (hasAuthor ? layout.author.height : 0)
            + (hasAuthor && hasDynasty ? settings.metadataInlineSpacing : 0)
            + (hasDynasty ? layout.dynasty.height : 0);
    }

    if (!layout.tag.isEmpty()) {
        layout.totalWidth = qMax(layout.totalWidth, layout.tag.width);
        layout.totalHeight += layout.tag.height;
    }
    if (!layout.title.isEmpty()) {
        if (layout.totalHeight > 0) layout.totalHeight += qMax(4, settings.titleMetadataSpacing / 2);
        layout.totalWidth = qMax(layout.totalWidth, layout.title.width);
        layout.totalHeight += layout.title.height;
    }
    if (layout.metadataHeight > 0) {
        if (layout.totalHeight > 0) layout.totalHeight += settings.titleMetadataSpacing;
        layout.totalWidth = qMax(layout.totalWidth, layout.metadataWidth);
        layout.totalHeight += layout.metadataHeight;
    }
    if (!layout.content.isEmpty()) {
        if (layout.totalHeight > 0) {
            layout.totalHeight += layout.metadataHeight > 0
                ? settings.metadataContentSpacing : settings.titleMetadataSpacing;
        }
        layout.totalWidth = qMax(layout.totalWidth, layout.content.width);
        layout.totalHeight += layout.content.height;
    }
    return layout;
}

// Splits a poem into the one or two blocks the renderer has to place.
QVector<PoemPart> poemParts(const Poem &poem, const RenderSettings &settings) {
    const QPair<QString, QString> labels = relationLabels(poem.relation);
    QVector<PoemPart> parts;

    PoemPart first;
    first.tag = settings.relationTagEnabled ? labels.first : QString();
    first.title = poem.title;
    first.author = poem.author;
    first.dynasty = poem.dynasty;
    first.content = poem.content;
    parts.append(first);

    if (poem.isPaired()) {
        PoemPart second;
        second.tag = settings.relationTagEnabled ? labels.second : QString();
        second.title = poem.secondTitle;
        second.author = poem.secondAuthor;
        second.dynasty = poem.secondDynasty;
        second.content = poem.secondContent;
        parts.append(second);
    }
    return parts;
}


void drawWrapped(QPainter &painter, const WrappedText &text, int x, int y, bool shadowEnabled) {
    if (text.isEmpty()) return;
    // Layout is measured with text.font, so drawing must use that exact font as
    // well. Without this call only the panel/layout changed with the size
    // control while glyphs were rendered with QPainter's small default font.
    painter.setFont(text.font);
    const QFontMetrics metrics(text.font);
    for (int i = 0; i < text.lines.size(); ++i) {
        const QString &line = text.lines.at(i);
        if (line.isEmpty()) continue;
        const int baseline = y + i * text.lineHeight + metrics.ascent();
        if (shadowEnabled) {
            QColor shadow(0, 0, 0, qMin(220, text.color.alpha()));
            painter.setPen(shadow);
            painter.drawText(x + 2, baseline + 2, line);
        }
        painter.setPen(text.color);
        painter.drawText(x, baseline, line);
    }
}

// Advances exactly the pixels buildBlock measured, so layout and painting cannot
// drift apart when a block grows a tag or a second half.
void drawBlock(QPainter &painter, const BlockLayout &block, int x, int y, const RenderSettings &effective) {
    bool placed = false;
    if (!block.tag.isEmpty()) {
        drawWrapped(painter, block.tag, x, y, effective.shadowEnabled);
        y += block.tag.height;
        placed = true;
    }
    if (!block.title.isEmpty()) {
        if (placed) y += qMax(4, effective.titleMetadataSpacing / 2);
        drawWrapped(painter, block.title, x, y, effective.shadowEnabled);
        y += block.title.height;
        placed = true;
    }
    if (block.metadataHeight > 0) {
        if (placed) y += effective.titleMetadataSpacing;
        if (block.metadataInline) {
            const int markX = x + block.author.width + effective.metadataInlineSpacing;
            drawWrapped(painter, block.author, x, y, effective.shadowEnabled);
            drawWrapped(painter, block.separator, markX, y, effective.shadowEnabled);
            drawWrapped(painter, block.dynasty, markX + block.separator.width + effective.metadataInlineSpacing,
                        y, effective.shadowEnabled);
        } else {
            int cursor = y;
            if (!block.author.isEmpty()) {
                drawWrapped(painter, block.author, x, cursor, effective.shadowEnabled);
                cursor += block.author.height;
                if (!block.dynasty.isEmpty()) cursor += effective.metadataInlineSpacing;
            }
            if (!block.dynasty.isEmpty()) drawWrapped(painter, block.dynasty, x, cursor, effective.shadowEnabled);
        }
        y += block.metadataHeight;
        placed = true;
    }
    if (!block.content.isEmpty()) {
        if (placed) y += block.metadataHeight > 0 ? effective.metadataContentSpacing : effective.titleMetadataSpacing;
        drawWrapped(painter, block.content, x, y, effective.shadowEnabled);
    }
}

QString resolveFontFamily(const QString &requested, QString *warning) {    const QStringList families = QFontDatabase().families();
    const QString requestedTrimmed = requested.trimmed();
    if (!requestedTrimmed.isEmpty() && families.contains(requestedTrimmed)) return requestedTrimmed;

    const QStringList candidates = {
        QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"),
        QStringLiteral("SimSun"), QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Arial")
    };
    for (const QString &candidate : candidates) {
        if (families.contains(candidate)) {
            if (warning) *warning = QStringLiteral("\u5b57\u4f53\u4e0d\u53ef\u7528\uff0c\u5df2\u56de\u9000\u5230\uff1a%1").arg(candidate);
            return candidate;
        }
    }
    const QString fallback = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    if (warning) *warning = QStringLiteral("\u5b57\u4f53\u4e0d\u53ef\u7528\uff0c\u5df2\u56de\u9000\u5230\uff1a%1").arg(fallback);
    return fallback;
}

} // namespace

QStringList wrapPoemText(const QString &text, const QFontMetrics &metrics, int maxWidth) {
    QStringList result;
    const QStringList paragraphs = text.split(QChar('\n'), Qt::KeepEmptyParts);
    for (const QString &paragraph : paragraphs) {
        if (paragraph.isEmpty()) {
            result.append(QString());
            continue;
        }

        QString current;
        for (const QChar ch : paragraph) {
            const QString candidate = current + ch;
            if (current.isEmpty() || metrics.horizontalAdvance(candidate) <= maxWidth) {
                current = candidate;
                continue;
            }
            // The break falls here. Carry the offending neighbour onto the next
            // line with it rather than stranding it: a lone 。 heading a line is
            // the defect users actually see, and a 「 left at the tail is its twin.
            QString next(ch);
            while (current.size() > 1 && endsLineForbidden(current.back())) {
                next.prepend(current.back());
                current.chop(1);
            }
            if (current.size() > 1 && startsLineForbidden(ch)) {
                next.prepend(current.back());
                current.chop(1);
            }
            result.append(current);
            current = next;
        }
        if (!current.isEmpty()) result.append(current);
    }
    if (result.isEmpty()) result.append(QString());
    return result;
}

QImage WallpaperRenderer::prepareBackground(const QImage &source, const RenderSettings &settings, const QSize &targetSize) {
    QImage canvas(targetSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(settings.canvasBackgroundColor);
    if (source.isNull() || !targetSize.isValid()) return canvas;

    // No convertToFormat here: QImage::scaled() already produces the format it
    // needs, so converting first allocated a second full-size copy of the source
    // (33 MB on a 4K panel) purely to be discarded one line later.
    QImage image = source;
    if (settings.fitMode == FitMode::Stretch) {
        image = image.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QPainter painter(&canvas);
        painter.drawImage(QPoint(0, 0), image);
        return canvas;
    }

    const Qt::AspectRatioMode mode = settings.fitMode == FitMode::Fill
        ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
    image = image.scaled(targetSize, mode, Qt::SmoothTransformation);
    QPainter painter(&canvas);
    const int x = (targetSize.width() - image.width()) / 2;
    const int y = (targetSize.height() - image.height()) / 2;
    painter.drawImage(QPoint(x, y), image);
    return canvas;
}

QRect WallpaperRenderer::textAreaForAnchor(const QSize &canvas, Anchor anchor, const QSize &textSize, int margin) {
    const int xRight = canvas.width() - margin - textSize.width();
    const int yBottom = canvas.height() - margin - textSize.height();
    switch (anchor) {
    case Anchor::TopLeft: return QRect(margin, margin, textSize.width(), textSize.height());
    case Anchor::TopRight: return QRect(xRight, margin, textSize.width(), textSize.height());
    case Anchor::Center: return QRect((canvas.width() - textSize.width()) / 2, (canvas.height() - textSize.height()) / 2, textSize.width(), textSize.height());
    case Anchor::BottomLeft: return QRect(margin, yBottom, textSize.width(), textSize.height());
    case Anchor::BottomRight: return QRect(xRight, yBottom, textSize.width(), textSize.height());
    }
    return QRect(xRight, yBottom, textSize.width(), textSize.height());
}

QImage WallpaperRenderer::render(const QImage &source, const Poem &poem, const RenderSettings &settings,
                                 const QSize &targetSize, QString *error) const {
    if (error) error->clear();
    if (!targetSize.isValid() || targetSize.width() <= 0 || targetSize.height() <= 0) {
        if (error) *error = QStringLiteral("\u76ee\u6807\u753b\u5e03\u5c3a\u5bf8\u65e0\u6548");
        return {};
    }
    if (source.isNull()) {
        if (error) *error = QStringLiteral("\u6e90\u56fe\u7247\u4e3a\u7a7a");
        return {};
    }
    if (poem.content.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("\u8bd7\u8bcd\u6b63\u6587\u4e3a\u7a7a");
        return {};
    }

    QImage canvas = prepareBackground(source, settings, targetSize);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QString warning;
    const QString family = resolveFontFamily(settings.fontFamily, &warning);
    const int margin = qMax(16, qRound(qMin(canvas.width(), canvas.height()) * 0.06));
    const int maxWidth = qMax(120, qRound(canvas.width() * (settings.anchor == Anchor::Center ? 0.82 : 0.54)));
    const int maxHeight = qMax(100, qRound(canvas.height() * (settings.anchor == Anchor::Center ? 0.78 : 0.78)));

    // Point sizes are logical values, but the wallpaper may be rendered at
    // 1080p, 1440p, 4K, or a high-DPI work area. Scale typography with the
    // target canvas so a 36 pt default does not become visually tiny on 4K.
    const qreal resolutionScale = qBound<qreal>(0.85,
        qSqrt((canvas.width() * qreal(canvas.height())) / (1920.0 * 1080.0)), 2.4);
    RenderSettings effective = settings;
    effective.titleMetadataSpacing = qMax(4, qRound(settings.titleMetadataSpacing * resolutionScale));
    effective.metadataContentSpacing = qMax(6, qRound(settings.metadataContentSpacing * resolutionScale));
    effective.metadataInlineSpacing = qMax(4, qRound(settings.metadataInlineSpacing * resolutionScale));
    effective.pairSpacing = qMax(8, qRound(settings.pairSpacing * resolutionScale));

    const QVector<PoemPart> parts = poemParts(poem, effective);
    const int panelPad = margin / 2;
    // Separate panels each carry their own padding, so the gap between blocks has
    // to absorb twice that; otherwise the two cards overlap each other.
    const bool separatePanels = parts.size() > 1 && effective.pairLayoutPanel == PairLayoutPanel::Separate;
    const int blockGap = separatePanels ? effective.pairSpacing + 2 * panelPad : effective.pairSpacing;
    QVector<BlockLayout> blocks;
    int combinedWidth = 0;
    int combinedHeight = 0;
    bool fits = false;
    const int requestedSize = qBound(10, qRound(settings.fontPointSize * resolutionScale), 180);
    for (int size = requestedSize; size >= 10; --size) {
        blocks.clear();
        combinedWidth = 0;
        combinedHeight = 0;
        for (const PoemPart &part : parts) {
            const BlockLayout block = buildBlock(part, effective, family, maxWidth, size);
            if (combinedHeight > 0) combinedHeight += blockGap;
            combinedHeight += block.totalHeight;
            combinedWidth = qMax(combinedWidth, block.totalWidth);
            blocks.append(block);
        }
        if (combinedHeight <= maxHeight && combinedWidth <= maxWidth) {
            fits = true;
            break;
        }
    }
    if (!fits) {
        if (error) *error = QStringLiteral("\u8bd7\u8bcd\u5185\u5bb9\u8fc7\u957f\uff0c\u5373\u4f7f\u7f29\u5c0f\u5230\u6700\u4f4e\u5b57\u53f7\u4ecd\u65e0\u6cd5\u653e\u5165\u5b89\u5168\u533a\u57df");
        return {};
    }

    const QSize contentSize(qMax(1, combinedWidth), qMax(1, combinedHeight));
    const QRect positioned = textAreaForAnchor(canvas.size(), effective.anchor, contentSize, margin);
    if (effective.panelEnabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(effective.panelColor);
        const int radius = qMax(6, margin / 4);
        if (separatePanels) {
            int cursor = positioned.top();
            for (const BlockLayout &block : blocks) {
                const QRect card(QPoint(positioned.left(), cursor), QSize(block.totalWidth, block.totalHeight));
                painter.drawRoundedRect(card.adjusted(-panelPad, -panelPad, panelPad, panelPad).intersected(canvas.rect()),
                                       radius, radius);
                cursor += block.totalHeight + blockGap;
            }
        } else {
            painter.drawRoundedRect(
                positioned.adjusted(-panelPad, -panelPad, panelPad, panelPad).intersected(canvas.rect()), radius, radius);
        }
    }

    int y = positioned.top();
    for (int i = 0; i < blocks.size(); ++i) {
        if (i > 0) y += blockGap;
        drawBlock(painter, blocks.at(i), positioned.left(), y, effective);
        y += blocks.at(i).totalHeight;
    }
    painter.end();
    if (error && !warning.isEmpty()) *error = warning;
    return canvas;
}

} // namespace poetry
