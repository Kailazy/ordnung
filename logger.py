import logging
import os
from logging.handlers import RotatingFileHandler
from paths import get_data_dir

LOG_DIR = os.path.join(get_data_dir(), "logs")
LOG_FILE = os.path.join(LOG_DIR, "eyebags.log")

# 5 MB per file, keep 5 backups = 25 MB max
MAX_BYTES = 5 * 1024 * 1024
BACKUP_COUNT = 5

_initialized = False


def setup_logging():
    global _initialized
    if _initialized:
        return
    _initialized = True

    os.makedirs(LOG_DIR, exist_ok=True)

    formatter = logging.Formatter(
        "%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    file_handler = RotatingFileHandler(
        LOG_FILE, maxBytes=MAX_BYTES, backupCount=BACKUP_COUNT, encoding="utf-8",
    )
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)

    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    console_handler.setFormatter(formatter)

    root = logging.getLogger()
    root.setLevel(logging.DEBUG)
    root.addHandler(file_handler)
    root.addHandler(console_handler)

    # Quiet down noisy libs
    logging.getLogger("werkzeug").setLevel(logging.WARNING)
    logging.getLogger("watchdog").setLevel(logging.WARNING)


def get_logger(name):
    setup_logging()
    return logging.getLogger(name)
