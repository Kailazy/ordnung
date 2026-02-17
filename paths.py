import os
import sys


def is_bundled():
    """Check if running as a PyInstaller bundle."""
    return getattr(sys, "frozen", False)


def get_base_dir():
    """Directory containing bundled resources (templates, static).

    In dev mode: the project directory.
    When bundled: the PyInstaller temp extraction directory.
    """
    if is_bundled():
        return sys._MEIPASS
    return os.path.dirname(os.path.abspath(__file__))


def get_data_dir():
    """Directory for persistent user data (database, logs).

    In dev mode: the project directory.
    When bundled: the directory containing the .exe.
    """
    if is_bundled():
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))
