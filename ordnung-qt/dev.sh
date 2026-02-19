#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Debug -Wno-dev 2>&1 | grep -v "^--"
cmake --build build -j$(nproc)
./build/Ordnung
