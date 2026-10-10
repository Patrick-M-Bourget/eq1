#!/usr/bin/env bash
# The build-and-test gate the hooks share: builds the working tree and runs every test but the slow
# frequency response grids ([response]), which are left to CI. Its output goes to a log: only the
# summary on success, all of it on failure. Usage: .githooks/gate.sh <hook name>
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

hook=$1
BUILD_DIR=${BUILD_DIR:-build}
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "$hook: no build in $BUILD_DIR yet; run scripts/check.sh build first" >&2
    exit 1
fi
log="$BUILD_DIR/$hook.log"
printf '\n== Build and tests (%s), log in %s\n' "$hook" "$log"
if ! { cmake --build "$BUILD_DIR" --config Release --parallel \
    && ctest --test-dir "$BUILD_DIR" -C Release -LE response -j 8 --output-on-failure; } > "$log" 2>&1; then
    cat "$log" >&2
    exit 1
fi
grep -E '^[0-9]+% tests passed' "$log"
