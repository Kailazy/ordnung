#pragma once

#include <string>

// Pure C++ — zero Qt headers. Trivially testable without QApplication.
struct Track {
    long long   id         = 0;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    double      bpm        = 0.0;
    int         rating     = 0;      // 0-5
    std::string time;                // duration string "M:SS"
    std::string key_sig;
    std::string date_added;
    std::string format;              // mp3, flac, wav, aiff, ...
    bool        has_aiff   = false;
    std::string match_key;           // lower(artist) + "|||" + lower(title)

    // Runtime-only UI state — not persisted
    bool        expanded   = false;
};
