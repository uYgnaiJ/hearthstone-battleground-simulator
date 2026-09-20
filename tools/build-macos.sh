#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo "This script requires macOS." >&2; exit 1; }
raylib="$PWD/native/vendor/raylib-6.0"
[[ -f "$raylib/src/Makefile" && -f native/vendor/json.hpp ]] || {
    echo "Run tools/setup-macos.sh first." >&2; exit 1;
}
# raylib 6.0 subtracts an unsigned window width when centering on macOS.
# A window wider than the monitor then overflows and Cocoa aborts.
if ! /usr/bin/grep -q 'monitorWidth - (int)CORE.Window.screen.width' "$raylib/src/platforms/rcore_desktop_glfw.c"; then
    patch -d "$raylib" -p1 < tools/raylib-macos-window.patch
    touch "$raylib/src/rcore.c"
fi
export MACOSX_DEPLOYMENT_TARGET=11.0
make -C "$raylib/src" -j "${JOBS:-4}" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
app="$PWD/release/macos/Battlegrounds.app"
resources="$app/Contents/Resources"
mkdir -p "$app/Contents/MacOS" "$resources/data" "$resources/licenses" test-results
for name in cards heroes token-art manifest; do
    cp "src/data/$name.json" "$resources/data/"
done
ditto native/assets "$resources/assets"
ditto public/art "$resources/art"
cp "$raylib/LICENSE" "$resources/licenses/raylib.txt"
cp native/vendor/json.hpp "$resources/licenses/nlohmann-json.hpp"
cp native/ASSETS.md "$resources/licenses/"
cp README.md "$resources/"
cat > "$app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>Battlegrounds</string>
<key>CFBundleIdentifier</key><string>local.bobsworkshop.battlegrounds</string>
<key>CFBundleName</key><string>Battlegrounds</string>
<key>CFBundleDisplayName</key><string>Battlegrounds — The Local Tavern</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.1.0</string>
<key>CFBundleVersion</key><string>1</string>
<key>LSMinimumSystemVersion</key><string>11.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
# Preserve the reference engine's separate floating-point operations on ARM.
"${CXX:-clang++}" -std=c++20 -O2 -ffp-contract=off -I"$raylib/src" native/main.cpp "$raylib/src/libraylib.a" \
    -framework Cocoa -framework IOKit -framework OpenGL -framework CoreVideo \
    -o "$app/Contents/MacOS/Battlegrounds"
if [[ "${1:-}" == --tests ]]; then
    "${CXX:-clang++}" -std=c++20 -O2 -ffp-contract=off native/tests.cpp -o test-results/native-tests
    "${CXX:-clang++}" -std=c++20 -O2 -I"$raylib/src" native/tests/perspective.cpp "$raylib/src/libraylib.a" \
        -framework Cocoa -framework IOKit -framework OpenGL -framework CoreVideo -o test-results/perspective-tests
fi
codesign --force --sign - "$app"
archive="$PWD/release/macos/Battlegrounds-macos-$(uname -m).zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$archive"
echo "Built $app"
echo "Packaged $archive"
