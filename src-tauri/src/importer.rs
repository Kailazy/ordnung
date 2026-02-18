use std::path::Path;
use crate::error::AppError;

/// A single parsed track row from a Rekordbox export file.
#[derive(Debug, Clone)]
pub struct TrackRow {
    pub title: String,
    pub artist: String,
    pub album: String,
    pub genre: String,
    pub bpm: String,
    pub rating: String,
    pub time: String,
    pub key: String,
    pub date_added: String,
}

/// Parse a Rekordbox tab-separated export file.
///
/// The TSV columns (0-indexed):
/// 0=# (row number), 1=TrackNumber, 2=Title, 3=Artist, 4=Album,
/// 5=Genre, 6=BPM, 7=Rating (star chars), 8=Time, 9=Key, 10=Date Added.
///
/// Detects encoding via BOM: UTF-16LE/BE, UTF-8-sig, or plain UTF-8.
pub fn parse_rekordbox_txt(path: &Path) -> Result<Vec<TrackRow>, AppError> {
    let raw = std::fs::read(path)?;
    let content = decode_with_bom(&raw);

    let lines: Vec<&str> = content.lines().collect();
    if lines.is_empty() {
        return Ok(Vec::new());
    }

    let mut tracks = Vec::new();

    // Skip header line (index 0)
    for line in &lines[1..] {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        let mut parts: Vec<&str> = line.split('\t').collect();
        // Pad to at least 11 columns
        while parts.len() < 11 {
            parts.push("");
        }

        // Parse rating: count '*' characters in parts[7]
        let rating_raw = parts[7].trim();
        let star_count = rating_raw.chars().filter(|&c| c == '*').count();
        let rating = if star_count > 0 {
            star_count.to_string()
        } else {
            String::new()
        };

        tracks.push(TrackRow {
            title: parts[2].trim().to_string(),
            artist: parts[3].trim().to_string(),
            album: parts[4].trim().to_string(),
            genre: parts[5].trim().to_string(),
            bpm: parts[6].trim().to_string(),
            rating,
            time: parts[8].trim().to_string(),
            key: parts[9].trim().to_string(),
            date_added: parts[10].trim().to_string(),
        });
    }

    Ok(tracks)
}

/// Detect encoding via BOM and decode bytes to a String.
fn decode_with_bom(raw: &[u8]) -> String {
    // Check for UTF-16 LE BOM (FF FE)
    if raw.len() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE {
        let (decoded, _, _) = encoding_rs::UTF_16LE.decode(&raw[2..]);
        return decoded.into_owned();
    }

    // Check for UTF-16 BE BOM (FE FF)
    if raw.len() >= 2 && raw[0] == 0xFE && raw[1] == 0xFF {
        let (decoded, _, _) = encoding_rs::UTF_16BE.decode(&raw[2..]);
        return decoded.into_owned();
    }

    // Check for UTF-8 BOM (EF BB BF)
    if raw.len() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF {
        return String::from_utf8_lossy(&raw[3..]).into_owned();
    }

    // Default: UTF-8
    String::from_utf8_lossy(raw).into_owned()
}
