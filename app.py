import os
import sys
import tempfile
from flask import Flask, render_template, request, jsonify, g
from paths import get_base_dir, is_bundled
from db import get_connection, init_db, _make_match_key
from importer import parse_rekordbox_txt
from watcher import FolderWatcher
from converter import ConversionWorker
from logger import get_logger

log = get_logger("app")

base_dir = get_base_dir()
app = Flask(
    __name__,
    template_folder=os.path.join(base_dir, "templates"),
    static_folder=os.path.join(base_dir, "static"),
)

folder_watcher = FolderWatcher()
conversion_worker = ConversionWorker()


# ── database helper ──────────────────────────────────────────────────────────

def get_db():
    if "db" not in g:
        g.db = get_connection()
    return g.db


@app.teardown_appcontext
def close_db(exc):
    db = g.pop("db", None)
    if db is not None:
        db.close()


# ── pages ────────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("base.html")


# ── playlist API ─────────────────────────────────────────────────────────────

@app.route("/api/playlists", methods=["GET"])
def list_playlists():
    db = get_db()
    playlists = db.execute(
        "SELECT id, name, imported_at FROM playlists ORDER BY imported_at DESC"
    ).fetchall()
    result = []
    for p in playlists:
        row = dict(p)
        counts = db.execute("""
            SELECT s.format, COUNT(*) as count
            FROM playlist_songs ps
            JOIN songs s ON s.id = ps.song_id
            WHERE ps.playlist_id = ?
            GROUP BY s.format
        """, (p["id"],)).fetchall()
        row["total"] = sum(c["count"] for c in counts)
        row["format_counts"] = {c["format"]: c["count"] for c in counts}
        result.append(row)
    return jsonify(result)


@app.route("/api/playlists/import", methods=["POST"])
def import_playlist():
    if "file" not in request.files:
        return jsonify({"error": "No file uploaded"}), 400

    f = request.files["file"]
    name = request.form.get("name", "").strip()
    if not name:
        name = os.path.splitext(f.filename)[0]

    tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".txt")
    try:
        f.save(tmp.name)
        tmp.close()
        tracks = parse_rekordbox_txt(tmp.name)
    finally:
        os.unlink(tmp.name)

    if not tracks:
        return jsonify({"error": "No tracks found in file"}), 400

    db = get_db()
    cur = db.execute("INSERT INTO playlists (name) VALUES (?)", (name,))
    playlist_id = cur.lastrowid

    added = 0
    for t in tracks:
        mk = _make_match_key(t["artist"], t["title"])

        db.execute("""
            INSERT OR IGNORE INTO songs
                (title, artist, album, genre, bpm, rating, time, key_sig, date_added, format, has_aiff, match_key)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'mp3', 0, ?)
        """, (t["title"], t["artist"], t["album"], t["genre"],
              t["bpm"], t["rating"], t["time"], t["key"], t["date_added"], mk))

        song_row = db.execute("SELECT id FROM songs WHERE match_key = ?", (mk,)).fetchone()
        if song_row:
            db.execute("""
                INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id)
                VALUES (?, ?)
            """, (playlist_id, song_row["id"]))
            added += 1

    db.commit()
    log.info("Imported playlist '%s': %d tracks (%d new songs)", name, len(tracks), added)
    return jsonify({"id": playlist_id, "name": name, "track_count": len(tracks)})


@app.route("/api/playlists/<int:pid>", methods=["DELETE"])
def delete_playlist(pid):
    db = get_db()
    db.execute("DELETE FROM playlist_songs WHERE playlist_id = ?", (pid,))
    db.execute("DELETE FROM playlists WHERE id = ?", (pid,))
    db.commit()
    return jsonify({"ok": True})


@app.route("/api/playlists/<int:pid>/tracks", methods=["GET"])
def get_tracks(pid):
    db = get_db()
    rows = db.execute("""
        SELECT s.id AS song_id, s.title, s.artist, s.album, s.genre,
               s.bpm, s.rating, s.time, s.key_sig, s.date_added,
               s.format, s.has_aiff, ps.id AS ps_id
        FROM playlist_songs ps
        JOIN songs s ON s.id = ps.song_id
        WHERE ps.playlist_id = ?
        ORDER BY ps.id
    """, (pid,)).fetchall()
    return jsonify([dict(r) for r in rows])


@app.route("/api/playlists/<int:pid>/export", methods=["GET"])
def export_tracks(pid):
    fmt = request.args.get("format", "")
    db = get_db()
    if fmt:
        rows = db.execute("""
            SELECT s.title, s.artist
            FROM playlist_songs ps
            JOIN songs s ON s.id = ps.song_id
            WHERE ps.playlist_id = ? AND s.format = ?
            ORDER BY ps.id
        """, (pid, fmt)).fetchall()
    else:
        rows = db.execute("""
            SELECT s.title, s.artist
            FROM playlist_songs ps
            JOIN songs s ON s.id = ps.song_id
            WHERE ps.playlist_id = ?
            ORDER BY ps.id
        """, (pid,)).fetchall()
    lines = []
    for r in rows:
        if r["artist"]:
            lines.append(f"{r['artist']} - {r['title']}")
        else:
            lines.append(r["title"])
    return jsonify({"tracks": lines, "count": len(lines)})


# ── song API (global, shared across playlists) ───────────────────────────────

@app.route("/api/songs/<int:sid>/format", methods=["PUT"])
def update_song_format(sid):
    data = request.get_json()
    fmt = data.get("format", "mp3").strip().lower()
    if not fmt:
        return jsonify({"error": "Format cannot be empty"}), 400
    db = get_db()
    db.execute("UPDATE songs SET format = ? WHERE id = ?", (fmt, sid))
    db.commit()
    return jsonify({"ok": True})


@app.route("/api/songs/<int:sid>/has-aiff", methods=["PUT"])
def update_song_aiff(sid):
    data = request.get_json()
    val = 1 if data.get("has_aiff") else 0
    db = get_db()
    db.execute("UPDATE songs SET has_aiff = ? WHERE id = ?", (val, sid))
    db.commit()
    return jsonify({"ok": True})


@app.route("/api/songs/bulk-format", methods=["PUT"])
def bulk_update_format():
    data = request.get_json()
    fmt = data.get("format", "mp3").strip().lower()
    ids = data.get("ids", None)
    if not fmt:
        return jsonify({"error": "Format cannot be empty"}), 400
    db = get_db()
    if ids:
        placeholders = ",".join("?" * len(ids))
        db.execute(
            f"UPDATE songs SET format = ? WHERE id IN ({placeholders})",
            [fmt] + ids,
        )
    else:
        playlist_id = data.get("playlist_id")
        if playlist_id:
            db.execute("""
                UPDATE songs SET format = ?
                WHERE id IN (SELECT song_id FROM playlist_songs WHERE playlist_id = ?)
            """, (fmt, playlist_id))
        else:
            db.execute("UPDATE songs SET format = ?", (fmt,))
    db.commit()
    return jsonify({"ok": True})


@app.route("/api/songs/<int:sid>/playlists", methods=["GET"])
def get_song_playlists(sid):
    db = get_db()
    all_playlists = db.execute("SELECT id, name FROM playlists ORDER BY name").fetchall()
    member_ids = set()
    rows = db.execute("SELECT playlist_id FROM playlist_songs WHERE song_id = ?", (sid,)).fetchall()
    for r in rows:
        member_ids.add(r["playlist_id"])
    result = []
    for p in all_playlists:
        result.append({
            "id": p["id"],
            "name": p["name"],
            "member": p["id"] in member_ids,
        })
    return jsonify(result)


@app.route("/api/songs/<int:sid>/playlists", methods=["PUT"])
def update_song_playlists(sid):
    data = request.get_json()
    add_ids = data.get("add", [])
    remove_ids = data.get("remove", [])
    db = get_db()
    for pid in add_ids:
        db.execute(
            "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id) VALUES (?, ?)",
            (pid, sid),
        )
    for pid in remove_ids:
        db.execute(
            "DELETE FROM playlist_songs WHERE playlist_id = ? AND song_id = ?",
            (pid, sid),
        )
    db.commit()
    return jsonify({"ok": True})


# ── legacy compat: redirect old track endpoints to song endpoints ─────────────

@app.route("/api/tracks/<int:tid>/format", methods=["PUT"])
def update_track_format_compat(tid):
    return update_song_format(tid)


@app.route("/api/tracks/bulk-format", methods=["PUT"])
def bulk_update_format_compat():
    return bulk_update_format()


# ── downloads / watcher API ──────────────────────────────────────────────────

@app.route("/api/downloads/config", methods=["GET"])
def get_watch_config():
    db = get_db()
    watch_row = db.execute("SELECT value FROM config WHERE key = 'watch_folder'").fetchone()
    output_row = db.execute("SELECT value FROM config WHERE key = 'output_folder'").fetchone()
    auto_row = db.execute("SELECT value FROM config WHERE key = 'auto_convert'").fetchone()
    return jsonify({
        "path": watch_row["value"] if watch_row else "",
        "output_folder": output_row["value"] if output_row else "",
        "auto_convert": (auto_row["value"] == "1") if auto_row else True,
        "active": folder_watcher.is_running,
        "converter_running": conversion_worker.is_running,
        "queue_size": conversion_worker.queue_size,
    })


@app.route("/api/downloads/config", methods=["PUT"])
def set_watch_config():
    data = request.get_json()
    db = get_db()

    path = data.get("path", "").strip()
    output_folder = data.get("output_folder", "").strip()
    auto_convert = data.get("auto_convert", None)

    if "path" in data:
        if not path:
            folder_watcher.stop()
            db.execute("DELETE FROM config WHERE key = 'watch_folder'")
        else:
            if not os.path.isdir(path):
                return jsonify({"error": f"Source directory not found: {path}"}), 400
            db.execute(
                "INSERT OR REPLACE INTO config (key, value) VALUES ('watch_folder', ?)",
                (path,),
            )

    if "output_folder" in data:
        if output_folder:
            os.makedirs(output_folder, exist_ok=True)
            db.execute(
                "INSERT OR REPLACE INTO config (key, value) VALUES ('output_folder', ?)",
                (output_folder,),
            )
        else:
            db.execute("DELETE FROM config WHERE key = 'output_folder'")

    if auto_convert is not None:
        db.execute(
            "INSERT OR REPLACE INTO config (key, value) VALUES ('auto_convert', ?)",
            ("1" if auto_convert else "0",),
        )

    db.commit()
    log.info("Config saved: watch=%s output=%s auto_convert=%s", path, output_folder, auto_convert)

    _restart_services()

    watch_row = db.execute("SELECT value FROM config WHERE key = 'watch_folder'").fetchone()
    output_row = db.execute("SELECT value FROM config WHERE key = 'output_folder'").fetchone()
    auto_row = db.execute("SELECT value FROM config WHERE key = 'auto_convert'").fetchone()

    return jsonify({
        "ok": True,
        "active": folder_watcher.is_running,
        "path": watch_row["value"] if watch_row else "",
        "output_folder": output_row["value"] if output_row else "",
        "auto_convert": (auto_row["value"] == "1") if auto_row else True,
        "converter_running": conversion_worker.is_running,
    })


@app.route("/api/downloads", methods=["GET"])
def list_downloads():
    db = get_db()
    rows = db.execute("""
        SELECT d.*,
               c.id AS conv_id, c.status AS conv_status,
               c.output_path AS conv_output, c.size_mb AS conv_size,
               c.error_msg AS conv_error
        FROM downloads d
        LEFT JOIN conversions c ON c.download_id = d.id
        ORDER BY d.detected_at DESC
        LIMIT 500
    """).fetchall()
    return jsonify([dict(r) for r in rows])


@app.route("/api/downloads/<int:did>", methods=["DELETE"])
def delete_download(did):
    db = get_db()
    db.execute("DELETE FROM conversions WHERE download_id = ?", (did,))
    db.execute("DELETE FROM downloads WHERE id = ?", (did,))
    db.commit()
    return jsonify({"ok": True})


@app.route("/api/downloads/scan", methods=["POST"])
def scan_folder():
    db = get_db()
    row = db.execute("SELECT value FROM config WHERE key = 'watch_folder'").fetchone()
    if not row:
        log.warning("Scan requested but no watch folder configured")
        return jsonify({"error": "No watch folder configured"}), 400
    path = row["value"]
    log.info("Scanning folder: %s", path)
    results = folder_watcher.scan_existing(path, None)
    added = 0
    to_convert = []
    for r in results:
        exists = db.execute(
            "SELECT 1 FROM downloads WHERE filepath = ?", (r["filepath"],)
        ).fetchone()
        if not exists:
            cur = db.execute(
                "INSERT INTO downloads (filename, filepath, extension, size_mb) VALUES (?, ?, ?, ?)",
                (r["filename"], r["filepath"], r["extension"], r["size_mb"]),
            )
            added += 1
            log.debug("New file detected: %s (%s)", r["filename"], r["extension"])
            to_convert.append((cur.lastrowid, r["filepath"], r["extension"]))
    db.commit()

    for download_id, filepath, extension in to_convert:
        _maybe_auto_convert(download_id, filepath, extension)

    log.info("Scan complete: %d found, %d new, %d queued for conversion", len(results), added, len(to_convert))
    return jsonify({"scanned": len(results), "added": added})


# ── activity log API ─────────────────────────────────────────────────────────

@app.route("/api/logs/recent", methods=["GET"])
def recent_logs():
    from logger import LOG_FILE
    n = request.args.get("n", 50, type=int)
    n = min(n, 200)
    lines = []
    try:
        with open(LOG_FILE, "r", encoding="utf-8") as f:
            all_lines = f.readlines()
            lines = [l.rstrip() for l in all_lines[-n:]]
    except FileNotFoundError:
        pass
    return jsonify({"lines": lines})


# ── conversion API ───────────────────────────────────────────────────────────

@app.route("/api/conversions", methods=["GET"])
def list_conversions():
    db = get_db()
    rows = db.execute(
        "SELECT * FROM conversions ORDER BY created_at DESC LIMIT 500"
    ).fetchall()
    return jsonify([dict(r) for r in rows])


@app.route("/api/conversions/stats", methods=["GET"])
def conversion_stats():
    db = get_db()
    rows = db.execute("""
        SELECT status, COUNT(*) as count FROM conversions GROUP BY status
    """).fetchall()
    stats = {r["status"]: r["count"] for r in rows}
    stats["queue_size"] = conversion_worker.queue_size
    stats["worker_running"] = conversion_worker.is_running
    return jsonify(stats)


@app.route("/api/conversions/retry/<int:cid>", methods=["POST"])
def retry_conversion(cid):
    db = get_db()
    row = db.execute("SELECT * FROM conversions WHERE id = ?", (cid,)).fetchone()
    if not row:
        return jsonify({"error": "Conversion not found"}), 404
    if row["status"] not in ("failed",):
        return jsonify({"error": "Can only retry failed conversions"}), 400

    output_dir = _get_output_folder()
    if not output_dir:
        return jsonify({"error": "No output folder configured"}), 400

    db.execute(
        "UPDATE conversions SET status='pending', error_msg='', started_at=NULL, finished_at=NULL WHERE id=?",
        (cid,),
    )
    db.commit()

    if conversion_worker.is_running:
        conversion_worker._queue.put((cid, row["source_path"], output_dir))

    return jsonify({"ok": True})


@app.route("/api/conversions/convert/<int:did>", methods=["POST"])
def convert_single(did):
    db = get_db()
    row = db.execute("SELECT * FROM downloads WHERE id = ?", (did,)).fetchone()
    if not row:
        return jsonify({"error": "Download not found"}), 404

    output_dir = _get_output_folder()
    if not output_dir:
        return jsonify({"error": "No output folder configured"}), 400

    _ensure_converter()
    conv_id = conversion_worker.enqueue(
        did, row["filepath"], row["extension"], output_dir,
    )
    return jsonify({"ok": True, "conversion_id": conv_id})


# ── folder picker ────────────────────────────────────────────────────────────

@app.route("/api/browse-folder", methods=["POST"])
def browse_folder():
    import threading

    result = {"path": ""}

    def _pick():
        try:
            import tkinter as tk
            from tkinter import filedialog
            root = tk.Tk()
            root.withdraw()
            root.attributes("-topmost", True)
            path = filedialog.askdirectory(title="Select folder")
            root.destroy()
            if path:
                result["path"] = path.replace("/", "\\")
        except Exception:
            pass

    t = threading.Thread(target=_pick)
    t.start()
    t.join(timeout=120)

    return jsonify(result)


# ── watcher + converter lifecycle ────────────────────────────────────────────

def _get_output_folder():
    conn = get_connection()
    row = conn.execute("SELECT value FROM config WHERE key = 'output_folder'").fetchone()
    conn.close()
    return row["value"] if row else ""


def _get_auto_convert():
    conn = get_connection()
    row = conn.execute("SELECT value FROM config WHERE key = 'auto_convert'").fetchone()
    conn.close()
    if not row:
        return True
    return row["value"] == "1"


def _ensure_converter():
    if not conversion_worker.is_running:
        try:
            conversion_worker.start()
        except RuntimeError:
            pass


def _maybe_auto_convert(download_id, filepath, extension):
    if not _get_auto_convert():
        return
    output_dir = _get_output_folder()
    if not output_dir:
        return
    _ensure_converter()
    conversion_worker.enqueue(download_id, filepath, extension, output_dir)


def _on_new_audio_file(filename, filepath, extension, size_mb):
    log.info("Watcher detected: %s (%.1f MB, %s)", filename, size_mb, extension)
    conn = get_connection()
    try:
        exists = conn.execute(
            "SELECT 1 FROM downloads WHERE filepath = ?", (filepath,)
        ).fetchone()
        if exists:
            log.debug("Already in DB, skipping: %s", filename)
            return
        cur = conn.execute(
            "INSERT INTO downloads (filename, filepath, extension, size_mb) VALUES (?, ?, ?, ?)",
            (filename, filepath, extension, size_mb),
        )
        conn.commit()
        download_id = cur.lastrowid
        log.debug("Added to downloads (id=%d): %s", download_id, filename)
        _maybe_auto_convert(download_id, filepath, extension)
    except Exception as e:
        log.error("Error processing new file %s: %s", filename, e, exc_info=True)
    finally:
        conn.close()


def _start_watcher(path):
    try:
        folder_watcher.start(path, _on_new_audio_file)
        log.info("Folder watcher started: %s", path)
    except ValueError as e:
        log.error("Failed to start watcher on %s: %s", path, e)


def _restart_services():
    conn = get_connection()
    watch_row = conn.execute("SELECT value FROM config WHERE key = 'watch_folder'").fetchone()
    conn.close()

    watch_path = watch_row["value"] if watch_row else ""
    if watch_path and os.path.isdir(watch_path):
        _start_watcher(watch_path)
    else:
        folder_watcher.stop()

    output_dir = _get_output_folder()
    if output_dir:
        _ensure_converter()


def _restore_on_startup():
    log.info("Restoring state from database...")
    conn = get_connection()
    watch_row = conn.execute("SELECT value FROM config WHERE key = 'watch_folder'").fetchone()
    conn.close()
    if watch_row and watch_row["value"] and os.path.isdir(watch_row["value"]):
        _start_watcher(watch_row["value"])
    else:
        log.info("No watch folder to restore")

    output_dir = _get_output_folder()
    if output_dir:
        _ensure_converter()
        conn = get_connection()
        pending = conn.execute(
            "SELECT id, source_path FROM conversions WHERE status IN ('pending','converting')"
        ).fetchall()
        if pending:
            log.info("Re-queuing %d interrupted conversions", len(pending))
        for row in pending:
            conn.execute("UPDATE conversions SET status='pending' WHERE id=?", (row["id"],))
            conversion_worker._queue.put((row["id"], row["source_path"], output_dir))
        conn.commit()
        conn.close()
    else:
        log.info("No output folder configured, converter not started")


# ── port finder ──────────────────────────────────────────────────────────────

def _find_free_port(start=5000):
    """Find an available port starting from `start`."""
    import socket
    for port in range(start, start + 100):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(("127.0.0.1", port))
                return port
            except OSError:
                continue
    return start


# ── main ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    init_db()
    log.info("=" * 60)
    log.info("Eyebags Terminal starting")
    log.info("=" * 60)
    _restore_on_startup()

    dev_mode = "--dev" in sys.argv

    if dev_mode:
        log.info("Running in dev mode (browser)")
        log.info("Server ready at http://localhost:5000")
        app.run(debug=True, port=5000, use_reloader=False)
    else:
        import threading
        import time

        try:
            import webview
        except ImportError:
            log.warning("pywebview not installed, falling back to browser mode")
            log.info("Install it with: pip install pywebview")
            log.info("Server ready at http://localhost:5000")
            app.run(debug=True, port=5000, use_reloader=False)
            sys.exit(0)

        port = _find_free_port()

        def start_flask():
            app.run(host="127.0.0.1", port=port, debug=False, use_reloader=False)

        flask_thread = threading.Thread(target=start_flask, daemon=True)
        flask_thread.start()
        time.sleep(0.5)

        log.info("Opening application window on port %d", port)
        window = webview.create_window(
            "Eyebags Terminal",
            f"http://127.0.0.1:{port}",
            width=1200,
            height=800,
            min_size=(800, 500),
        )
        webview.start(gui="qt")

        log.info("Window closed, shutting down")
        folder_watcher.stop()
        conversion_worker.stop()
