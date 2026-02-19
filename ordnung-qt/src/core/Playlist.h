#pragma once

#include <string>
#include <map>

struct Playlist {
    long long   id            = 0;
    std::string name;
    std::string imported_at;
    int         total         = 0;
    std::map<std::string, int> format_counts;  // {"mp3": 42, "flac": 8, ...}
};
