import os
import time
import threading
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
from logger import get_logger

log = get_logger("watcher")

AUDIO_EXTENSIONS = {".mp3", ".flac", ".wav", ".aiff", ".aif", ".ogg", ".m4a", ".wma", ".alac"}


class AudioFileHandler(FileSystemEventHandler):
    def __init__(self, on_new_file):
        super().__init__()
        self.on_new_file = on_new_file

    def _wait_for_file_ready(self, filepath, timeout=30):
        """Wait until the file size stabilizes (download complete)."""
        prev_size = -1
        stable_count = 0
        start = time.time()
        while time.time() - start < timeout:
            try:
                size = os.path.getsize(filepath)
            except OSError:
                return False
            if size == prev_size and size > 0:
                stable_count += 1
                if stable_count >= 3:
                    return True
            else:
                stable_count = 0
            prev_size = size
            time.sleep(0.5)
        return prev_size > 0

    def on_created(self, event):
        if event.is_directory:
            return
        ext = os.path.splitext(event.src_path)[1].lower()
        if ext in AUDIO_EXTENSIONS:
            log.debug("File created event: %s", event.src_path)
            self._wait_for_file_ready(event.src_path)
            try:
                size_bytes = os.path.getsize(event.src_path)
            except OSError:
                size_bytes = 0
            self.on_new_file(
                filename=os.path.basename(event.src_path),
                filepath=event.src_path,
                extension=ext.lstrip("."),
                size_mb=round(size_bytes / (1024 * 1024), 2),
            )


class FolderWatcher:
    def __init__(self):
        self._observer = None
        self._thread = None
        self._watch_path = None
        self._on_new_file = None

    @property
    def is_running(self):
        return self._observer is not None and self._observer.is_alive()

    @property
    def watch_path(self):
        return self._watch_path

    def start(self, path, on_new_file):
        self.stop()
        if not os.path.isdir(path):
            raise ValueError(f"Not a valid directory: {path}")
        self._watch_path = path
        self._on_new_file = on_new_file
        handler = AudioFileHandler(on_new_file)
        self._observer = Observer()
        self._observer.schedule(handler, path, recursive=False)
        self._observer.daemon = True
        self._observer.start()

    def stop(self):
        if self._observer and self._observer.is_alive():
            self._observer.stop()
            self._observer.join(timeout=3)
        self._observer = None
        self._watch_path = None

    def scan_existing(self, path, on_new_file):
        """One-time scan of existing audio files in a folder."""
        if not os.path.isdir(path):
            return []
        results = []
        for fname in os.listdir(path):
            fpath = os.path.join(path, fname)
            if os.path.isfile(fpath):
                ext = os.path.splitext(fname)[1].lower()
                if ext in AUDIO_EXTENSIONS:
                    try:
                        size_bytes = os.path.getsize(fpath)
                    except OSError:
                        size_bytes = 0
                    results.append({
                        "filename": fname,
                        "filepath": fpath,
                        "extension": ext.lstrip("."),
                        "size_mb": round(size_bytes / (1024 * 1024), 2),
                    })
        return results
