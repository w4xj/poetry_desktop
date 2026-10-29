#pragma once

#include <QString>

namespace poetry {

// Owns the per-user Run entry. The app deliberately does not use a scheduled
// task: a plain Run value needs no extra privileges and is easy to remove.
class WindowsAutoStart final {
public:
    static bool setEnabled(bool enabled, QString *error = nullptr);
    // Exposed for tests and for repairing a stale entry after the exe moves.
    static QString commandLine(const QString &executablePath);
};

} // namespace poetry
