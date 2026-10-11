#!/usr/bin/env bash
# The build-and-test gate the hooks share: builds the working tree and runs every test but the slow
# frequency response grids ([response]), which are left to CI. Its output goes to a log: only the
# summary on success, all of it on failure. A pass records the tree it tested (the staged one, which the
# pre-commit hook makes the working tree's), so pre-push can skip a tree that already passed.
# Usage: .githooks/gate.sh <hook name>
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

hook=$1
BUILD_DIR=${BUILD_DIR:-build}
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "$hook: no build in $BUILD_DIR yet; run scripts/check.sh build first" >&2
    exit 1
fi
log="$BUILD_DIR/$hook.log"

# At most GATE_SLOTS (3) gates build at once across every worktree: past that, an 8-core Mac slows each one
# several times over and the editor tests' timing flakes (docs/agents/builder.md, "Running several builders").
# A slot is a directory in the shared git dir holding its gate's PID; a slot whose gate died is reclaimed.
slots="$(git rev-parse --path-format=absolute --git-common-dir)/gate-slots"
mkdir -p "$slots"
slot= waited=
until [ -n "$slot" ]; do
    for ((n = 1; n <= ${GATE_SLOTS:-3}; n++)); do
        if mkdir "$slots/$n" 2> /dev/null; then
            echo $$ > "$slots/$n/pid"
            slot=$slots/$n
            break
        fi
        pid=$(cat "$slots/$n/pid" 2> /dev/null || true)
        if [ -n "$pid" ] && ! kill -0 "$pid" 2> /dev/null; then
            rm -rf "$slots/$n"
            n=$((n - 1)) # try that slot again
        fi
    done
    if [ -z "$slot" ]; then
        [ -z "$waited" ] && echo "$hook: ${GATE_SLOTS:-3} gates are already building; waiting for one to finish" >&2
        waited=1
        sleep 5
    fi
done
trap 'rm -rf "$slot"' EXIT

printf '\n== Build and tests (%s), log in %s\n' "$hook" "$log"
if ! { cmake --build "$BUILD_DIR" --config Release --parallel \
    && ctest --test-dir "$BUILD_DIR" -C Release -LE response -j 8 --output-on-failure; } > "$log" 2>&1; then
    cat "$log" >&2
    exit 1
fi
grep -E '^[0-9]+% tests passed' "$log"
git write-tree > "$BUILD_DIR/gate-passed-tree"
