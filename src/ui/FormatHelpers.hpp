#pragma once
#include <QString>

namespace ui {

inline QString formatBytes(qint64 bytes)
{
    if (bytes < 0)        return "?";
    if (bytes < 1024)     return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1 << 20)  return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1 << 30)  return QStringLiteral("%1 MB").arg(bytes / (1 << 20));
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1 << 30), 0, 'f', 1);
}

inline QString formatDuration(qint64 ms)
{
    if (ms < 0) return "?";
    const auto s   = ms / 1000;
    const auto min = s / 60;
    const auto sec = s % 60;
    if (min > 0) return QStringLiteral("%1m %2s").arg(min).arg(sec);
    return QStringLiteral("%1s").arg(sec);
}

} // namespace ui
