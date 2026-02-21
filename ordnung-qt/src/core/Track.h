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
    std::string filepath;            // absolute path to audio file on disk

    // Extended metadata (Rekordbox-level)
    int         color_label = 0;    // 0 = none, 1-8 = Pioneer color index
    int         bitrate     = 0;    // kbps
    std::string comment;
    int         play_count  = 0;
    std::string date_played;
    int         energy      = 0;    // 1-10 intensity

    // Essentia deep analysis results
    std::string mood_tags;              // comma-separated mood tags from Discogs-Effnet
    std::string style_tags;             // comma-separated style/genre tags from model
    float       danceability = 0.0f;    // 0.0–1.0
    float       valence      = 0.0f;    // 0.0–1.0 (positivity)
    float       vocal_prob   = 0.0f;    // 0.0–1.0 (1.0 = vocal, 0.0 = instrumental)
    bool        essentia_analyzed = false;

    // Preparation mode — persisted
    bool        is_prepared  = false;  // DJ has marked this track as prepared for a gig

    // Runtime-only UI state — not persisted
    bool        expanded     = false;
    bool        is_analyzing = false;  // true while background ffprobe is running for this track
};
