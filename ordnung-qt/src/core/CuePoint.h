#pragma once
#include <string>

// Type of a cue point on a track.
enum class CueType {
    HotCue,     // One of 8 hot cue slots (A-H), triggered from CDJ pads
    MemoryCue,  // Unnamed memory cue (no pad slot)
    Loop        // Loop marker with start + end positions
};

// A single cue/loop marker stored in the cue_points table.
struct CuePoint {
    long long   id          = 0;
    long long   song_id     = 0;
    CueType     cue_type    = CueType::HotCue;
    int         slot        = -1;       // 0-7 for hot cues, -1 for memory cues / loops
    int         position_ms = 0;        // start position in milliseconds from track start
    int         end_ms      = -1;       // loop end in ms; -1 if not a loop
    std::string name;                   // user label ("Drop", "Chorus", ...)
    int         color       = 1;        // 1-8 Pioneer color index
    int         sort_order  = 0;        // display/sort order among cues for this track
};
