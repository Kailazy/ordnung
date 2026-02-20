#!/usr/bin/env bash
# Downloads pre-built ONNX Runtime release for the current platform
# and installs into third_party/onnxruntime/<triplet>/.
#
# Run on each platform (Linux/WSL, macOS, Windows/Git Bash) once, then commit.
# ONNX Runtime releases: https://github.com/microsoft/onnxruntime/releases
set -euo pipefail

ORT_VERSION="1.20.1"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
  Linux*)
    TRIPLET="linux-x64"
    TARNAME="onnxruntime-linux-x64-${ORT_VERSION}.tgz"
    ;;
  Darwin*)
    if [ "$ARCH" = "arm64" ]; then
        TRIPLET="macos-arm64"
        TARNAME="onnxruntime-osx-arm64-${ORT_VERSION}.tgz"
    else
        TRIPLET="macos-x64"
        TARNAME="onnxruntime-osx-x86_64-${ORT_VERSION}.tgz"
    fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    TRIPLET="windows-x64"
    TARNAME="onnxruntime-win-x64-${ORT_VERSION}.zip"
    ;;
  *)
    echo "Unsupported OS: $OS" >&2
    exit 1
    ;;
esac

URL="https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/${TARNAME}"
DEST="$REPO_ROOT/third_party/onnxruntime/$TRIPLET"
TMP_DIR=$(mktemp -d /tmp/ort-XXXXXX)
trap 'rm -rf "$TMP_DIR"' EXIT

echo "==> Downloading ONNX Runtime ${ORT_VERSION} for ${TRIPLET}..."
curl -fsSL "$URL" -o "$TMP_DIR/$TARNAME"

echo "==> Extracting..."
mkdir -p "$TMP_DIR/extracted"
if [[ "$TARNAME" == *.zip ]]; then
    unzip -q "$TMP_DIR/$TARNAME" -d "$TMP_DIR/extracted"
else
    tar -xf "$TMP_DIR/$TARNAME" -C "$TMP_DIR/extracted" --strip-components=1 2>/dev/null || \
    (mkdir -p "$TMP_DIR/extracted" && tar -xf "$TMP_DIR/$TARNAME" -C "$TMP_DIR/extracted")
fi

# The tarball extracts to onnxruntime-<os>-<arch>-<ver>/include and /lib
EXTRACTED_DIR=$(find "$TMP_DIR/extracted" -maxdepth 1 -type d | tail -1)
if [ -d "$TMP_DIR/extracted/include" ]; then
    EXTRACTED_DIR="$TMP_DIR/extracted"
fi

mkdir -p "$DEST/include" "$DEST/lib"
cp -r "$EXTRACTED_DIR/include/"* "$DEST/include/"
cp -P "$EXTRACTED_DIR/lib/"* "$DEST/lib/" 2>/dev/null || true

# ── Windows/MinGW: official ORT ships an MSVC-format .lib that MinGW cannot
#    use. Generate a MinGW-compatible import lib via gendef + dlltool instead.
if [[ "$OS" == MINGW* || "$OS" == MSYS* || "$OS" == CYGWIN* ]]; then
    ORT_DLL="$DEST/lib/onnxruntime.dll"
    if [ -f "$ORT_DLL" ]; then
        echo "==> Generating MinGW import library (libonnxruntime.dll.a)..."
        cd "$DEST/lib"
        gendef onnxruntime.dll
        dlltool -D onnxruntime.dll -d onnxruntime.def -l libonnxruntime.dll.a
        rm -f onnxruntime.def
        cd - > /dev/null
    fi
fi

# ── macOS: normalise the dylib install name so it is relocatable ──────────
# The official ORT release stores an absolute or versioned install name.
# We rewrite it to @rpath/<filename> so the app binary's RPATH resolves it
# correctly regardless of where the dylib ends up in the bundle.
if [[ "$OS" == Darwin* ]]; then
    ORT_DYLIB=$(find "$DEST/lib" -name "libonnxruntime*.dylib" ! -type l | head -1)
    if [ -n "$ORT_DYLIB" ]; then
        BASENAME=$(basename "$ORT_DYLIB")
        echo "==> Normalising $BASENAME install name to @rpath/$BASENAME ..."
        install_name_tool -id "@rpath/$BASENAME" "$ORT_DYLIB"
    fi
fi

echo ""
echo "Done. ONNX Runtime installed to: $DEST"
echo "Run: git add third_party/onnxruntime/$TRIPLET && git commit -m 'chore: add bundled ONNX Runtime $TRIPLET'"
