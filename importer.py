import csv
import io


def detect_encoding(filepath):
    with open(filepath, "rb") as f:
        raw = f.read(4)
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
        return "utf-16"
    if raw[:3] == b"\xef\xbb\xbf":
        return "utf-8-sig"
    return "utf-8"


def parse_rekordbox_txt(filepath):
    """Parse a Rekordbox tab-separated export file.

    Returns a list of dicts with keys:
        title, artist, album, genre, bpm, rating, time, key, date_added
    """
    encoding = detect_encoding(filepath)

    with open(filepath, "r", encoding=encoding) as f:
        content = f.read()

    lines = content.splitlines()
    if not lines:
        return []

    tracks = []
    for line in lines[1:]:
        line = line.strip()
        if not line:
            continue

        parts = line.split("\t")
        while len(parts) < 11:
            parts.append("")

        rating_raw = parts[7].strip()
        star_count = rating_raw.count("*")
        rating = str(star_count) if star_count > 0 else ""

        tracks.append({
            "title": parts[2].strip(),
            "artist": parts[3].strip(),
            "album": parts[4].strip(),
            "genre": parts[5].strip(),
            "bpm": parts[6].strip(),
            "rating": rating,
            "time": parts[8].strip(),
            "key": parts[9].strip(),
            "date_added": parts[10].strip(),
        })

    return tracks


def tracks_to_csv(tracks, output_path):
    """Write parsed tracks to a CSV file."""
    headers = ["Track Title", "Artist", "Album", "Genre", "BPM", "Rating",
               "Time", "Key", "Date Added"]
    with open(output_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        for t in tracks:
            writer.writerow([
                t["title"], t["artist"], t["album"], t["genre"],
                t["bpm"], t["rating"], t["time"], t["key"], t["date_added"]
            ])
