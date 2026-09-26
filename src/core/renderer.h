#pragma once

#include "models.h"

#include <QImage>
#include <QSize>
#include <QString>

namespace poetry {

// Wraps text to maxWidth, honouring the Chinese typesetting rules that forbid
// closing punctuation at the start of a line and opening punctuation at its end.
// Exposed so the line-breaking rules can be tested without rendering.
QStringList wrapPoemText(const QString &text, const QFontMetrics &metrics, int maxWidth);

class WallpaperRenderer {
public:
    QImage render(const QImage &source, const Poem &poem, const RenderSettings &settings,
                  const QSize &targetSize, QString *error = nullptr) const;

private:
    static QRect textAreaForAnchor(const QSize &canvas, Anchor anchor, const QSize &textSize, int margin);
    static QImage prepareBackground(const QImage &source, const RenderSettings &settings, const QSize &targetSize);
};

} // namespace poetry
