import os
import shutil
import subprocess
import threading
import time
import queue
from db import get_connection
from logger import get_logger

log = get_logger("converter")

FFMPEG_BIN = "ffmpeg"


def find_ffmpeg():
    """Locate ffmpeg binary, checking common install paths on Windows."""
    for candidate in [
        FFMPEG_BIN,
        r"C:\ProgramData\chocolatey\bin\ffmpeg.exe",
        os.path.expandvars(r"%LOCALAPPDATA%\Microsoft\WinGet\Links\ffmpeg.exe"),
    ]:
        try:
            result = subprocess.run(
                [candidate, "-version"],
                capture_output=True, text=True, timeout=5,
            )
            if result.returncode == 0:
                return candidate
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    return None


def convert_to_aiff(source_path, output_dir, ffmpeg_bin=None):
    """Convert an audio file to AIFF using FFmpeg.

    Returns (output_path, error_msg). On success error_msg is empty.
    If the source is already AIFF, copies it instead.
    """
    if not ffmpeg_bin:
        ffmpeg_bin = find_ffmpeg()
    if not ffmpeg_bin:
        return "", "ffmpeg not found"

    if not os.path.isfile(source_path):
        return "", f"source file not found: {source_path}"

    os.makedirs(output_dir, exist_ok=True)

    base = os.path.splitext(os.path.basename(source_path))[0]
    output_path = os.path.join(output_dir, base + ".aiff")

    # Avoid overwriting -- add a counter suffix if needed
    counter = 1
    while os.path.exists(output_path):
        output_path = os.path.join(output_dir, f"{base}_{counter}.aiff")
        counter += 1

    src_ext = os.path.splitext(source_path)[1].lower()

    # Already AIFF -- just copy
    if src_ext in (".aiff", ".aif"):
        import shutil
        try:
            shutil.copy2(source_path, output_path)
            return output_path, ""
        except Exception as e:
            return "", str(e)

    # FFmpeg conversion: lossless PCM into AIFF container
    cmd = [
        ffmpeg_bin,
        "-i", source_path,
        "-c:a", "pcm_s16be",   # 16-bit big-endian PCM (standard AIFF)
        "-f", "aiff",
        "-y",
        output_path,
    ]

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=600,
        )
        if result.returncode != 0:
            stderr = result.stderr[-500:] if result.stderr else "unknown error"
            if os.path.exists(output_path):
                os.remove(output_path)
            return "", stderr
        return output_path, ""
    except subprocess.TimeoutExpired:
        if os.path.exists(output_path):
            os.remove(output_path)
        return "", "conversion timed out (10 min limit)"
    except Exception as e:
        return "", str(e)


class ConversionWorker:
    """Background thread that processes conversion jobs from a queue."""

    def __init__(self):
        self._queue = queue.Queue()
        self._thread = None
        self._running = False
        self._ffmpeg_bin = None

    @property
    def is_running(self):
        return self._running and self._thread and self._thread.is_alive()

    @property
    def queue_size(self):
        return self._queue.qsize()

    def start(self):
        if self.is_running:
            return
        self._ffmpeg_bin = find_ffmpeg()
        if not self._ffmpeg_bin:
            log.error("ffmpeg not found on this system")
            raise RuntimeError("ffmpeg not found on this system")
        log.info("Conversion worker started (ffmpeg=%s)", self._ffmpeg_bin)
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        self._queue.put(None)

    def enqueue(self, download_id, source_path, source_ext, output_dir):
        """Add a conversion job to the queue. Creates the DB record."""
        conn = get_connection()
        try:
            existing = conn.execute(
                "SELECT id FROM conversions WHERE source_path = ? AND status IN ('pending','converting','done')",
                (source_path,),
            ).fetchone()
            if existing:
                log.debug("Conversion already exists (id=%d) for %s, skipping", existing["id"], os.path.basename(source_path))
                return existing["id"]

            cur = conn.execute(
                "INSERT INTO conversions (download_id, source_path, source_ext, status) VALUES (?, ?, ?, 'pending')",
                (download_id, source_path, source_ext),
            )
            conv_id = cur.lastrowid
            conn.commit()
        finally:
            conn.close()

        log.info("Queued conversion #%d: %s -> aiff (output: %s)", conv_id, os.path.basename(source_path), output_dir)
        self._queue.put((conv_id, source_path, output_dir))
        return conv_id

    def _run(self):
        while self._running:
            try:
                job = self._queue.get(timeout=2)
            except queue.Empty:
                continue

            if job is None:
                break

            conv_id, source_path, output_dir = job
            self._process(conv_id, source_path, output_dir)

    def _process(self, conv_id, source_path, output_dir):
        fname = os.path.basename(source_path)
        log.info("Converting #%d: %s", conv_id, fname)
        conn = get_connection()
        try:
            conn.execute(
                "UPDATE conversions SET status='converting', started_at=CURRENT_TIMESTAMP WHERE id=?",
                (conv_id,),
            )
            conn.commit()

            start_time = time.time()
            output_path, error = convert_to_aiff(source_path, output_dir, self._ffmpeg_bin)
            elapsed = round(time.time() - start_time, 1)

            if error:
                log.error("Conversion #%d FAILED (%.1fs): %s -- %s", conv_id, elapsed, fname, error[:200])
                conn.execute(
                    "UPDATE conversions SET status='failed', error_msg=?, finished_at=CURRENT_TIMESTAMP WHERE id=?",
                    (error[:1000], conv_id),
                )
            else:
                try:
                    size_mb = round(os.path.getsize(output_path) / (1024 * 1024), 2)
                except OSError:
                    size_mb = 0
                log.info("Conversion #%d DONE (%.1fs): %s -> %s (%.1f MB)", conv_id, elapsed, fname, os.path.basename(output_path), size_mb)
                conn.execute(
                    "UPDATE conversions SET status='done', output_path=?, size_mb=?, finished_at=CURRENT_TIMESTAMP WHERE id=?",
                    (output_path, size_mb, conv_id),
                )

                # Move source file to _converted subfolder
                try:
                    source_dir = os.path.dirname(source_path)
                    converted_dir = os.path.join(source_dir, "_converted")
                    os.makedirs(converted_dir, exist_ok=True)
                    dest = os.path.join(converted_dir, fname)
                    if os.path.isfile(source_path):
                        shutil.move(source_path, dest)
                        conn.execute(
                            "UPDATE downloads SET filepath = ? WHERE filepath = ?",
                            (dest, source_path),
                        )
                        conn.execute(
                            "UPDATE conversions SET source_path = ? WHERE id = ?",
                            (dest, conv_id),
                        )
                        log.info("Moved source to _converted: %s", fname)
                except Exception as move_err:
                    log.warning("Could not move source to _converted: %s", move_err)

            conn.commit()
        except Exception as e:
            log.error("Conversion #%d EXCEPTION: %s", conv_id, e, exc_info=True)
            try:
                conn.execute(
                    "UPDATE conversions SET status='failed', error_msg=?, finished_at=CURRENT_TIMESTAMP WHERE id=?",
                    (str(e)[:1000], conv_id),
                )
                conn.commit()
            except Exception:
                pass
        finally:
            conn.close()
