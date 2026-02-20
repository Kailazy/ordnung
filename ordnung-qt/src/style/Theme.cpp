#include "style/Theme.h"

// =============================================================================
// Theme.cpp — Implements the badge color lookup functions.
//
// These are the only runtime functions in Theme — everything else in Theme.h
// is constexpr and needs no .cpp definition.
//
// Adding a new format or status: add it here. No other file changes required.
// =============================================================================

namespace Theme::Badge {

Colors forFormat(const QString& format)
{
    const QString f = format.toLower();

    // Formats that share semantic palette tokens
    if (f == QLatin1String("mp3"))
        return { QColor(Color::Text3),   QColor(Color::Bg3) };
    if (f == QLatin1String("flac"))
        return { QColor(Color::Accent),  QColor(Color::AccentBg) };
    if (f == QLatin1String("aiff") || f == QLatin1String("aif"))
        return { QColor(Color::Green),   QColor(Color::GreenBg) };
    if (f == QLatin1String("ogg"))
        return { QColor(Color::Amber),   QColor(Color::AmberBg) };
    if (f == QLatin1String("wma"))
        return { QColor(Color::Red),     QColor(Color::RedBg) };

    // Formats with dedicated identity hues
    if (f == QLatin1String("wav"))
        return { QColor(Color::FmtWav),  QColor(Color::FmtWavBg) };
    if (f == QLatin1String("m4a"))
        return { QColor(Color::FmtM4a),  QColor(Color::FmtM4aBg) };
    if (f == QLatin1String("alac"))
        return { QColor(Color::FmtAlac), QColor(Color::FmtAlacBg) };
    if (f == QLatin1String("aac"))
        return { QColor(Color::FmtAac),  QColor(Color::FmtAacBg) };

    // Fallback — unknown format, muted grey
    return { QColor(Color::Text3), QColor(Color::Bg3) };
}

Colors forStatus(const QString& status)
{
    const QString s = status.toLower();

    if (s == QLatin1String("pending"))
        return { QColor(Color::Amber),  QColor(Color::AmberBg) };
    if (s == QLatin1String("converting"))
        return { QColor(Color::Accent), QColor(Color::AccentBg) };
    if (s == QLatin1String("done"))
        return { QColor(Color::Green),  QColor(Color::GreenBg) };
    if (s == QLatin1String("failed"))
        return { QColor(Color::Red),    QColor(Color::RedBg) };

    // Fallback
    return { QColor(Color::Text3), QColor(Color::Bg3) };
}

} // namespace Theme::Badge
