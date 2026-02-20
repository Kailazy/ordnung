#!/usr/bin/env bash
# Builds Essentia + its dependencies for Windows (MinGW64) and installs into
# third_party/essentia/windows-x64/.
#
# Run this inside an MSYS2 MINGW64 shell on Windows:
#   1. Install MSYS2 from https://www.msys2.org/
#   2. Open "MSYS2 MinGW64" (NOT MSYS2 UCRT64 or CLANG64)
#   3. cd to the repo root and run:  bash scripts/build-essentia-windows.sh
#
# After this completes, commit third_party/essentia/windows-x64/ to Git LFS.
set -euo pipefail

# Verify we're in the right MSYS2 environment
if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
    echo "ERROR: Run this script inside an MSYS2 MINGW64 shell, not '${MSYSTEM:-plain bash}'." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO_ROOT/third_party/essentia/windows-x64"
BUILD_TMP=$(mktemp -d /tmp/essentia-build-XXXXXX)
trap 'rm -rf "$BUILD_TMP"' EXIT

echo "==> Installing MSYS2 build dependencies..."
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-python \
    mingw-w64-x86_64-fftw \
    mingw-w64-x86_64-yaml-cpp \
    mingw-w64-x86_64-libsamplerate \
    mingw-w64-x86_64-ffmpeg \
    mingw-w64-x86_64-eigen3 \
    git

echo "==> Cloning Essentia (shallow)..."
git clone --depth 1 https://github.com/MTG/essentia.git "$BUILD_TMP/src"

echo "==> Patching ALL Essentia wscripts: removing MSVC-only flags incompatible with MinGW..."
cd "$BUILD_TMP/src"
python - <<'PYEOF'
import re, glob, os

msvc_flags = ['/W2', '/EHsc', '/MD', '/GR', '/FS', '/bigobj']

# Find ALL Python files and wscripts in the tree
py_files = glob.glob('**/*.py', recursive=True) + glob.glob('**/wscript', recursive=True) + ['wscript']
py_files = [f for f in sorted(set(py_files)) if os.path.isfile(f)]

total_patched = 0
for fpath in py_files:
    try:
        with open(fpath, encoding='utf-8', errors='replace') as f:
            content = f.read()
    except OSError:
        continue
    if not any(flag in content for flag in msvc_flags):
        continue
    original = content
    for flag in msvc_flags:
        content = re.sub(r"""['"]""" + re.escape(flag) + r"""['"][,\s]*""", '', content)
    if content != original:
        with open(fpath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f'  Patched: {fpath}')
        total_patched += 1

if total_patched == 0:
    print('  No MSVC flag patterns found in any Python/wscript files.')
else:
    print(f'  Done — patched {total_patched} file(s).')
PYEOF

# ── g++ wrapper ────────────────────────────────────────────────────────────
# We can't reliably find every place Essentia injects MSVC flags (they may
# come from waf's own embedded tool code, or from Python modules the wscript
# imports). Instead we put a transparent Python proxy in front of g++ that
# strips any MSVC-only flag before forwarding the rest to the real compiler.
# This works regardless of where the flags originate.

echo "==> Installing transparent g++ wrapper (strips MSVC flags)..."

WRAPPER_DIR="$BUILD_TMP/bin"
WRAPPER_PY="$WRAPPER_DIR/gpp_wrapper.py"
mkdir -p "$WRAPPER_DIR"

# The wrapper Python script — intercepts every g++ call
cat > "$WRAPPER_PY" << 'PYEOF'
import sys, subprocess

# Real MinGW64 g++ binary — not on PATH so we don't recurse
REAL_GPP = '/mingw64/bin/g++.exe'

# MSVC-only flags that GCC/MinGW does not understand
MSVC_FLAGS = {
    '/W0', '/W1', '/W2', '/W3', '/W4', '/Wall',
    '/EHsc', '/EHs', '/EHa', '/EHac',
    '/MD', '/MDd', '/MT', '/MTd',
    '/GR', '/GR-', '/FS', '/bigobj',
    '/GS', '/GS-', '/sdl', '/sdl-',
    '/Zi', '/ZI', '/Od', '/O1', '/O2',
}

filtered = [a for a in sys.argv[1:] if a not in MSVC_FLAGS]
sys.exit(subprocess.call([REAL_GPP] + filtered))
PYEOF

# .bat launcher — Windows CreateProcess can execute .bat files directly,
# so Python subprocess (which uses CreateProcess) will find and run this
# when it looks for 'g++' on PATH.
WIN_PYTHON=$(cygpath -w "$(which python)")
WIN_WRAPPER=$(cygpath -w "$WRAPPER_PY")
printf '@"%s" "%s" %%*\r\n' "$WIN_PYTHON" "$WIN_WRAPPER" > "$WRAPPER_DIR/g++.bat"

# Verify the wrapper is found before the real g++
export PATHEXT=".COM;.EXE;.BAT;.CMD"
export PATH="$WRAPPER_DIR:$PATH"
echo "  Wrapper location: $(which g++)"
echo "  Real g++ at: /mingw64/bin/g++.exe (called by wrapper)"

# ── Configure ──────────────────────────────────────────────────────────────
echo "==> Configuring Essentia with waf..."
cd "$BUILD_TMP/src"
python waf configure \
    --mode=release \
    --check-cxx-compiler=g++ \
    --check-c-compiler=gcc \
    --prefix="$BUILD_TMP/install"

echo "==> Building Essentia..."
python waf build -j"$(nproc)"
python waf install

echo "==> Copying to third_party/essentia/windows-x64/ ..."
mkdir -p "$DEST/include" "$DEST/lib"
cp -r "$BUILD_TMP/install/include/essentia" "$DEST/include/"

# Copy the DLL and generate a MinGW-compatible import lib (.dll.a)
# The waf build produces essentia.dll in build/src/
ESSENTIA_DLL=$(find "$BUILD_TMP/src/build" -name "essentia.dll" | head -1)
if [ -z "$ESSENTIA_DLL" ]; then
    ESSENTIA_DLL=$(find "$BUILD_TMP/install" -name "essentia.dll" | head -1)
fi

if [ -z "$ESSENTIA_DLL" ]; then
    echo "ERROR: essentia.dll not found after build. Check waf output above." >&2
    exit 1
fi

cp "$ESSENTIA_DLL" "$DEST/lib/essentia.dll"

echo "==> Generating MinGW import library (essentia.dll.a)..."
cd "$DEST/lib"
gendef essentia.dll
dlltool -D essentia.dll -d essentia.def -l libessentia.dll.a
rm -f essentia.def

echo "==> Copying runtime DLL dependencies from MSYS2..."
ldd "$DEST/lib/essentia.dll" \
    | grep -i '/mingw64/' \
    | awk '{print $3}' \
    | sort -u \
    | while read -r dep; do
        [ -f "$dep" ] && cp -n "$dep" "$DEST/lib/" && echo "  copied: $(basename "$dep")"
    done

echo ""
echo "Done. Files installed to: $DEST"
echo ""
echo "Next steps:"
echo "  1. Run: bash scripts/fetch-onnxruntime.sh  (from Git Bash or this MSYS2 shell)"
echo "  2. git add third_party/essentia/windows-x64 third_party/onnxruntime/windows-x64"
echo "  3. git commit -m 'chore: add bundled Essentia + ONNX Runtime windows-x64'"
