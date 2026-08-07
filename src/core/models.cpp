#include "models.h"

#include <QJsonArray>

namespace poetry {

QString fitModeToString(FitMode mode) {
    switch (mode) {
    case FitMode::Fill: return QStringLiteral("fill");
    case FitMode::Fit: return QStringLiteral("fit");
    case FitMode::Stretch: return QStringLiteral("stretch");
    }
    return QStringLiteral("fill");
}

FitMode fitModeFromString(const QString &value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("fit")) return FitMode::Fit;
    if (normalized == QStringLiteral("stretch")) return FitMode::Stretch;
    return FitMode::Fill;
}

QString anchorToString(Anchor anchor) {
    switch (anchor) {
    case Anchor::TopLeft: return QStringLiteral("topLeft");
    case Anchor::TopRight: return QStringLiteral("topRight");
    case Anchor::Center: return QStringLiteral("center");
    case Anchor::BottomLeft: return QStringLiteral("bottomLeft");
    case Anchor::BottomRight: return QStringLiteral("bottomRight");
    }
    return QStringLiteral("bottomRight");
}

Anchor anchorFromString(const QString &value) {
    if (value == QStringLiteral("topLeft")) return Anchor::TopLeft;
    if (value == QStringLiteral("topRight")) return Anchor::TopRight;
    if (value == QStringLiteral("center")) return Anchor::Center;
    if (value == QStringLiteral("bottomLeft")) return Anchor::BottomLeft;
    return Anchor::BottomRight;
}

QJsonObject poemToJson(const Poem &poem) {
    QJsonObject object;
    object.insert(QStringLiteral("id"), poem.id);
    object.insert(QStringLiteral("title"), poem.title);
    object.insert(QStringLiteral("author"), poem.author);
    object.insert(QStringLiteral("dynasty"), poem.dynasty);
    object.insert(QStringLiteral("content"), poem.content);
    QJsonArray tags;
    for (const QString &tag : poem.tags) tags.append(tag);
    object.insert(QStringLiteral("tags"), tags);
    object.insert(QStringLiteral("enabled"), poem.enabled);
    return object;
}

Poem poemFromJson(const QJsonObject &object, QString *error) {
    Poem poem;
    auto fail = [&](const QString &message) {
        if (error) *error = message;
        return poem;
    };

    if (!object.value(QStringLiteral("id")).isUndefined() && !object.value(QStringLiteral("id")).isString())
        return fail(QStringLiteral("id 必须是字符串"));
    if (!object.value(QStringLiteral("title")).isUndefined() && !object.value(QStringLiteral("title")).isString())
        return fail(QStringLiteral("title 必须是字符串"));
    if (!object.value(QStringLiteral("author")).isUndefined() && !object.value(QStringLiteral("author")).isString())
        return fail(QStringLiteral("author 必须是字符串"));
    if (!object.value(QStringLiteral("dynasty")).isUndefined() && !object.value(QStringLiteral("dynasty")).isString())
        return fail(QStringLiteral("dynasty 必须是字符串"));
    if (!object.value(QStringLiteral("content")).isString())
        return fail(QStringLiteral("content 必须是字符串"));
    if (!object.value(QStringLiteral("tags")).isUndefined() && !object.value(QStringLiteral("tags")).isArray())
        return fail(QStringLiteral("tags 必须是数组"));
    if (!object.value(QStringLiteral("enabled")).isUndefined() && !object.value(QStringLiteral("enabled")).isBool())
        return fail(QStringLiteral("enabled 必须是布尔值"));

    poem.id = object.value(QStringLiteral("id")).toString().trimmed();
    poem.title = object.value(QStringLiteral("title")).toString();
    poem.author = object.value(QStringLiteral("author")).toString();
    poem.dynasty = object.value(QStringLiteral("dynasty")).toString();
    poem.content = object.value(QStringLiteral("content")).toString().trimmed();
    poem.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    for (const QJsonValue &tag : object.value(QStringLiteral("tags")).toArray()) {
        if (tag.isString()) poem.tags.append(tag.toString());
    }
    if (poem.content.isEmpty()) return fail(QStringLiteral("正文不能为空"));
    return poem;
}

} // namespace poetry
