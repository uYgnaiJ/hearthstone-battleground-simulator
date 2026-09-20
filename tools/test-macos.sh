#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "${1:-}" == --skip-build ]]; then shift; else bash tools/build-macos.sh --tests; fi
if [[ "${1:-}" == --soak ]]; then
    ./test-results/native-tests --soak "${2:-1000}"
else
    ./test-results/native-tests
    ./test-results/perspective-tests
fi
if [[ "${1:-}" == --smoke ]]; then
    run_dir="$(mktemp -d "$PWD/test-results/native-smoke-XXXXXX")"
    BOBS_DATA_DIR="$run_dir" ./release/macos/Battlegrounds.app/Contents/MacOS/Battlegrounds --smoke
    plutil -extract ok raw "$run_dir/smoke-result.json" | /usr/bin/grep -qx true
    cat "$run_dir/smoke-result.json"
fi
