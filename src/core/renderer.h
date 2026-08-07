#pragma once

#include "models.h"

#include <QImage>
#include <QSize>
#include <QString>

namespace poetry {

class WallpaperRenderer {
public:
    QImage render(const QImage &source, const Poem &poem, const RenderSettings &settings,
                  const QSize &targetSize, QString *error = nullptr) const;

private:
    static QRect textAreaForAnchor(const QSize &canvas, Anchor anchor, const QSize &textSize, int margin);
    static QImage prepareBackground(const QImage &source, const RenderSettings &settings, const QSize &targetSize);
};

} // namespace poetry
