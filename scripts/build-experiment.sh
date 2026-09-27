#!/bin/sh
# Usage: build-experiment.sh <preset> <target> [<target> ...]
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
preset=${1:-debug}
if [ "$#" -gt 0 ]; then shift; fi
case "$preset" in debug|release|asan) ;; *) echo "Unknown preset: $preset" >&2; exit 2 ;; esac
cmake_bin=${CMAKE:-cmake}
if ! command -v "$cmake_bin" >/dev/null 2>&1; then
    cmake_bin=/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake
    if [ ! -x "$cmake_bin" ]; then
        cmake_bin=/Applications/CLion.app/Contents/bin/cmake/mac/x64/bin/cmake
    fi
fi
cd "$root"
"$cmake_bin" --preset "$preset"
if [ "$#" -gt 0 ]; then
    "$cmake_bin" --build --preset "$preset" --parallel --target "$@"
else
    "$cmake_bin" --build --preset "$preset" --parallel
fi
