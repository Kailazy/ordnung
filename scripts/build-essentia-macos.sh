#!/usr/bin/env bash
# Builds Essentia for macOS (arm64 or x86_64) and installs into third_party/.
#
# Run on a Mac (arm64 or Intel) before committing third_party/ to Git LFS.
# Requires: Homebrew -- brew install fftw libyaml libsamplerate ffmpeg
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    TRIPLET="macos-arm64"
else
    TRIPLET="macos-x64"
fi
DEST="$REPO_ROOT/third_party/essentia/$TRIPLET"
BUILD_TMP=$(mktemp -d /tmp/essentia-build-XXXXXX)
trap 'rm -rf "$BUILD_TMP"' EXIT

echo "==> Installing Homebrew dependencies..."
brew install fftw libyaml libsamplerate ffmpeg python3

echo "==> Cloning Essentia (shallow)..."
git clone --depth 1 https://github.com/MTG/essentia.git "$BUILD_TMP/src"

echo "==> Configuring and building Essentia..."
cd "$BUILD_TMP/src"
python3 waf configure \
    --mode=release \
    --prefix="$BUILD_TMP/install"
python3 waf build -j"$(sysctl -n hw.logicalcpu)"
python3 waf install

echo "==> Copying to third_party/essentia/$TRIPLET/ ..."
mkdir -p "$DEST/include" "$DEST/lib"
cp -r "$BUILD_TMP/install/include/essentia" "$DEST/include/"
cp -P "$BUILD_TMP/install/lib"/libessentia*.dylib "$DEST/lib/" 2>/dev/null || \
  cp    "$BUILD_TMP/install/lib/libessentia.a"    "$DEST/lib/"

# ── Make the dylib fully portable (no Homebrew path dependencies) ─────────
# dylibbundler copies all non-system transitive deps alongside libessentia.dylib
# and rewrites their install names to @loader_path/ so they resolve at runtime
# from whatever directory they are deployed to.
echo "==> Installing dylibbundler..."
brew install dylibbundler

echo "==> Normalising libessentia.dylib install name..."
# Set the dylib's own id to @rpath/libessentia.dylib so the linker records
# a relocatable name in the app binary's load commands.
install_name_tool -id "@rpath/libessentia.dylib" "$DEST/lib/libessentia.dylib"

echo "==> Bundling transitive Homebrew dependencies..."
# -od  overwrite existing deps in the dest dir
# -b   update the binary (libessentia.dylib) with new dep paths
# -x   the dylib to inspect
# -d   destination directory for copied deps
# -p   path prefix to rewrite dep install names to
dylibbundler -od -b \
    -x "$DEST/lib/libessentia.dylib" \
    -d "$DEST/lib/" \
    -p "@loader_path/"

echo ""
echo "Done. Files installed to: $DEST"
echo "All transitive deps are bundled and use @loader_path/ install names."
echo "Run: git add third_party/essentia/$TRIPLET && git commit -m 'chore: add bundled Essentia $TRIPLET'"
