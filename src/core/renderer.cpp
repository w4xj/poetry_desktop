#include "renderer.h"

#include <QFontDatabase>
#include <QFontMetrics>
#include <QPainter>
#include <QStringList>

namespace poetry {

namespace {

struct WrappedText {
    QStringList lines;
    QFont font;
    QColor color;
    int lineHeight = 0;
    int width = 0;
    int height = 0;

    bool isEmpty() const { return lines.isEmpty(); }
};

QStringList wrapText(const QString &text, const QFontMetrics &metrics, int maxWidth) {
    QStringList result;
    const QString normalized = text;
    const QStringList paragraphs = normalized.split(QChar('\n'), Qt::KeepEmptyParts);
    for (const QString &paragraph : paragraphs) {
        if (paragraph.isEmpty()) {
            result.append(QString());
            continue;
        }

        QString current;
        for (const QChar ch : paragraph) {
            const QString candidate = current + ch;
            if (!current.isEmpty() && metrics.horizontalAdvance(candidate) > maxWidth) {
                result.append(current);
                current = QString(ch);
            } else {
                current = candidate;
            }
        }
        if (!current.isEmpty()) result.append(current);
    }
    if (result.isEmpty()) result.append(QString());
    return result;
}

WrappedText makeWrapped(const QString &text, const QFont &font, const QColor &color,
                        int maxWidth, qreal lineSpacing) {
    WrappedText wrapped;
    wrapped.font = font;
    wrapped.color = color;
    const QFontMetrics metrics(font);
    wrapped.lines = wrapText(text, metrics, maxWidth);
    wrapped.lineHeight = qMax(metrics.height(), qRound(metrics.height() * lineSpacing));
    for (const QString &line : wrapped.lines)
        wrapped.width = qMax(wrapped.width, metrics.horizontalAdvance(line));
    wrapped.height = wrapped.lines.size() * wrapped.lineHeight;
    return wrapped;
}

struct TypographyLayout {
    WrappedText title;
    WrappedText author;
    WrappedText dynasty;
    WrappedText content;
    bool metadataInline = false;
    int metadataWidth = 0;
    int metadataHeight = 0;
    int totalWidth = 0;
    int totalHeight = 0;
};

TypographyLayout buildLayout(const Poem &poem, const RenderSettings &settings,
                             const QString &family, int maxWidth, int basePointSize) {
    TypographyLayout layout;
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

    if (!poem.title.trimmed().isEmpty())
        layout.title = makeWrapped(poem.title.trimmed(), titleFont, settings.titleColor, maxWidth, 1.10);
    if (!poem.author.trimmed().isEmpty())
        layout.author = makeWrapped(poem.author.trimmed(), authorFont, settings.metadataColor, maxWidth, 1.10);
    if (!poem.dynasty.trimmed().isEmpty())
        layout.dynasty = makeWrapped(poem.dynasty.trimmed(), dynastyFont, settings.metadataColor, maxWidth, 1.10);
    layout.content = makeWrapped(poem.content.trimmed(), contentFont,
                                 settings.contentColor.isValid() ? settings.contentColor : settings.textColor,
                                 maxWidth, qMax<qreal>(1.0, settings.contentLineSpacing));

    const bool hasAuthor = !layout.author.isEmpty();
    const bool hasDynasty = !layout.dynasty.isEmpty();
    if (hasAuthor && hasDynasty && layout.author.lines.size() == 1 && layout.dynasty.lines.size() == 1
        && layout.author.width + settings.metadataInlineSpacing + layout.dynasty.width <= maxWidth) {
        layout.metadataInline = true;
        layout.metadataWidth = layout.author.width + settings.metadataInlineSpacing + layout.dynasty.width;
        layout.metadataHeight = qMax(layout.author.height, layout.dynasty.height);
    } else if (hasAuthor || hasDynasty) {
        layout.metadataWidth = qMax(hasAuthor ? layout.author.width : 0, hasDynasty ? layout.dynasty.width : 0);
        layout.metadataHeight = (hasAuthor ? layout.author.height : 0)
            + (hasAuthor && hasDynasty ? settings.metadataInlineSpacing : 0)
            + (hasDynasty ? layout.dynasty.height : 0);
    }

    if (!layout.title.isEmpty()) {
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

QString resolveFontFamily(const QString &requested, QString *warning) {
    const QStringList families = QFontDatabase().families();
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

QImage WallpaperRenderer::prepareBackground(const QImage &source, const RenderSettings &settings, const QSize &targetSize) {
    QImage canvas(targetSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(settings.canvasBackgroundColor);
    if (source.isNull() || !targetSize.isValid()) return canvas;

    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
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
    TypographyLayout layout;
    bool fits = false;
    const int requestedSize = qBound(10, qRound(settings.fontPointSize * resolutionScale), 180);
    for (int size = requestedSize; size >= 10; --size) {
        layout = buildLayout(poem, effective, family, maxWidth, size);
        if (layout.totalHeight <= maxHeight && layout.totalWidth <= maxWidth) {
            fits = true;
            break;
        }
    }
    if (!fits) {
        if (error) *error = QStringLiteral("\u8bd7\u8bcd\u5185\u5bb9\u8fc7\u957f\uff0c\u5373\u4f7f\u7f29\u5c0f\u5230\u6700\u4f4e\u5b57\u53f7\u4ecd\u65e0\u6cd5\u653e\u5165\u5b89\u5168\u533a\u57df");
        return {};
    }

    const QSize contentSize(qMax(1, layout.totalWidth), qMax(1, layout.totalHeight));
    const QRect positioned = textAreaForAnchor(canvas.size(), effective.anchor, contentSize, margin);
    QRect panelRect = positioned.adjusted(-margin / 2, -margin / 2, margin / 2, margin / 2).intersected(canvas.rect());
    if (effective.panelEnabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(effective.panelColor);
        painter.drawRoundedRect(panelRect, qMax(6, margin / 4), qMax(6, margin / 4));
    }

    int y = positioned.top();
    if (!layout.title.isEmpty()) {
        drawWrapped(painter, layout.title, positioned.left(), y, effective.shadowEnabled);
        y += layout.title.height;
        if (layout.metadataHeight > 0 || !layout.content.isEmpty()) y += effective.titleMetadataSpacing;
    }

    if (layout.metadataHeight > 0) {
        if (layout.metadataInline) {
            drawWrapped(painter, layout.author, positioned.left(), y, effective.shadowEnabled);
            drawWrapped(painter, layout.dynasty,
                        positioned.left() + layout.author.width + effective.metadataInlineSpacing,
                        y, settings.shadowEnabled);
        } else {
            if (!layout.author.isEmpty()) {
                drawWrapped(painter, layout.author, positioned.left(), y, effective.shadowEnabled);
                y += layout.author.height;
                if (!layout.dynasty.isEmpty()) y += effective.metadataInlineSpacing;
            }
            if (!layout.dynasty.isEmpty()) drawWrapped(painter, layout.dynasty, positioned.left(), y, effective.shadowEnabled);
        }
        y += layout.metadataHeight;
        if (!layout.content.isEmpty()) y += effective.metadataContentSpacing;
    }

    if (!layout.content.isEmpty()) drawWrapped(painter, layout.content, positioned.left(), y, effective.shadowEnabled);
    painter.end();
    if (error && !warning.isEmpty()) *error = warning;
    return canvas;
}

} // namespace poetry
