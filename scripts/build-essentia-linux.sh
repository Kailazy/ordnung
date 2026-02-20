#!/usr/bin/env bash
# Builds Essentia as a shared library for Linux x64 and installs it into
# third_party/essentia/linux-x64/ relative to the repo root.
#
# Run once from WSL/Linux before committing third_party/ to Git LFS.
# Requires: build-essential python3 libfftw3-dev libyaml-dev libsamplerate0-dev
#           libavcodec-dev libavformat-dev libavutil-dev libswresample-dev
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST="$REPO_ROOT/third_party/essentia/linux-x64"
BUILD_TMP=$(mktemp -d /tmp/essentia-build-XXXXXX)
trap 'rm -rf "$BUILD_TMP"' EXIT

echo "==> Installing build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y \
    build-essential python3 python3-dev \
    libfftw3-dev libyaml-dev libsamplerate0-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

echo "==> Cloning Essentia (shallow)..."
git clone --depth 1 https://github.com/MTG/essentia.git "$BUILD_TMP/src"

echo "==> Configuring and building Essentia..."
cd "$BUILD_TMP/src"
python3 waf configure \
    --mode=release \
    --prefix="$BUILD_TMP/install"
python3 waf build -j"$(nproc)"
python3 waf install

echo "==> Copying to third_party/essentia/linux-x64/ ..."
mkdir -p "$DEST/include" "$DEST/lib"
cp -r "$BUILD_TMP/install/include/essentia" "$DEST/include/"
# Copy all .so* files (versioned + unversioned symlink)
cp -P "$BUILD_TMP/install/lib"/libessentia.so* "$DEST/lib/"

echo ""
echo "Done. Files installed to: $DEST"
echo "Run: git add third_party/essentia/linux-x64 && git commit -m 'chore: add bundled Essentia linux-x64'"
