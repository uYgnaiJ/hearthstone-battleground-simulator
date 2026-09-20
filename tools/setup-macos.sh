#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo "This script requires macOS." >&2; exit 1; }
xcrun --find clang++ >/dev/null
mkdir -p native/vendor
download() {
    local url="$1" file="$2" hash="$3"
    if [[ ! -f "$file" ]]; then
        curl -fsSL --retry 3 "$url" -o "$file.download"
        mv "$file.download" "$file"
    fi
    echo "$hash  $file" | shasum -a 256 -c -
}
download https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz native/vendor/raylib-6.0.tar.gz 2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
download https://cdn.jsdelivr.net/gh/nlohmann/json@v3.12.0/single_include/nlohmann/json.hpp native/vendor/json.hpp aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63
if [[ ! -f native/vendor/raylib-6.0/src/Makefile ]]; then
    tar -xzf native/vendor/raylib-6.0.tar.gz -C native/vendor
fi
echo "macOS dependencies are ready."
