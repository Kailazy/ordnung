#pragma once

#include <string>

enum class ConversionStatus {
    Pending,
    Converting,
    Done,
    Failed,
    None   // no conversion record exists yet
};

struct ConversionJob {
    long long   id           = 0;
    long long   download_id  = 0;
    std::string source_path;
    std::string output_path;
    std::string source_ext;
    ConversionStatus status  = ConversionStatus::Pending;
    std::string error_msg;
    double      size_mb      = 0.0;
    std::string started_at;
    std::string finished_at;
};

struct Download {
    long long   id          = 0;
    std::string filename;
    std::string filepath;
    std::string extension;
    double      size_mb     = 0.0;
    std::string detected_at;

    // Joined from conversions table (may be absent)
    bool             has_conversion = false;
    long long        conv_id        = 0;
    ConversionStatus conv_status    = ConversionStatus::None;
    std::string      conv_error;
};
