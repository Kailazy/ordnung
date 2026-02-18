use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Playlist {
    pub id: i64,
    pub name: String,
    pub imported_at: String,
    pub total: i64,
    pub format_counts: serde_json::Value,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct PlaylistTrack {
    pub song_id: i64,
    pub title: String,
    pub artist: Option<String>,
    pub album: Option<String>,
    pub genre: Option<String>,
    pub bpm: Option<String>,
    pub rating: Option<String>,
    pub time: Option<String>,
    pub key_sig: Option<String>,
    pub date_added: Option<String>,
    pub format: String,
    pub has_aiff: i64,
    pub ps_id: i64,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Download {
    pub id: i64,
    pub filename: String,
    pub filepath: String,
    pub extension: Option<String>,
    pub size_mb: Option<f64>,
    pub detected_at: Option<String>,
    pub conv_id: Option<i64>,
    pub conv_status: Option<String>,
    pub conv_output: Option<String>,
    pub conv_size: Option<f64>,
    pub conv_error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Conversion {
    pub id: i64,
    pub download_id: Option<i64>,
    pub source_path: String,
    pub output_path: Option<String>,
    pub source_ext: Option<String>,
    pub status: String,
    pub error_msg: Option<String>,
    pub size_mb: Option<f64>,
    pub started_at: Option<String>,
    pub finished_at: Option<String>,
    pub created_at: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct WatchConfig {
    pub path: String,
    pub output_folder: String,
    pub auto_convert: bool,
    pub active: bool,
    pub converter_running: bool,
    pub queue_size: usize,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ConversionStats {
    pub pending: i64,
    pub converting: i64,
    pub done: i64,
    pub failed: i64,
    pub skipped: i64,
    pub queue_size: usize,
    pub worker_running: bool,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct SongPlaylist {
    pub id: i64,
    pub name: String,
    pub member: bool,
}
