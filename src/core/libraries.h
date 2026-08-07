#pragma once

#include "models.h"

#include <QVector>

#include <random>

namespace poetry {

class PoetryLibrary {
public:
    static PoetryLibrary fromPoems(const QVector<Poem> &poems);

    const QVector<Poem> &all() const { return m_poems; }
    QVector<Poem> &all() { return m_poems; }
    QVector<Poem> enabledPoems() const;

    ImportResult importJson(const QString &filePath, bool merge = true);
    bool setEnabled(const QString &id, bool enabled);
    bool remove(const QString &id);

    static QVector<Poem> defaultPoems();

private:
    QVector<Poem> m_poems;
};

class ImageLibrary {
public:
    struct ScanResult {
        QVector<ImageItem> items;
        int skipped = 0;
        QStringList errors;
    };

    static ScanResult scan(const QVector<ImageSource> &sources);
    void setItems(const QVector<ImageItem> &items) { m_items = items; }
    const QVector<ImageItem> &items() const { return m_items; }
    QVector<ImageItem> validItems() const;

private:
    QVector<ImageItem> m_items;
};

class RandomPicker {
public:
    RandomPicker();
    explicit RandomPicker(quint32 seed);

    void setSeed(quint32 seed);
    void setForcedIndices(const QVector<int> &indices);
    void clearForcedIndices();

    int pickIndex(int count, int lastIndex = -1);
    int pickPoem(const QVector<Poem> &poems);
    int pickImage(const QVector<ImageItem> &items);

    void rememberPoem(const QString &id) { m_lastPoemId = id; }
    void rememberImage(const QString &path) { m_lastImagePath = path; }
    QString lastPoemId() const { return m_lastPoemId; }
    QString lastImagePath() const { return m_lastImagePath; }

private:
    std::mt19937 m_generator;
    QVector<int> m_forcedIndices;
    int m_forcedPosition = 0;
    QString m_lastPoemId;
    QString m_lastImagePath;
};

} // namespace poetry
