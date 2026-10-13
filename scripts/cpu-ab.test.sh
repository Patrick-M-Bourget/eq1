#!/usr/bin/env bash
# Tests the CPU A/B's verdicts (scripts/cpu-ab.awk, which scripts/check.sh cpu <ref> runs) on made-up loads:
# a scene is "slower" or "faster" only when its median after/before ratio is at least the threshold from 1.0
# and enough rounds agree in direction; "no clear change" otherwise.
# Run: scripts/check.sh hooks (by hand, or in a full check; never from a git hook).
set -uo pipefail
awk_script=$(cd "$(dirname "$0")" && pwd)/cpu-ab.awk
failures=0

# Seven rounds of one scene, each "after before" load, through the A/B's summary with its thresholds
# (3%, 6 of 7 rounds); prints the scene's line.
summary() {
    local after before
    while [ $# -gt 0 ]; do
        after=$1 before=$2
        shift 2
        printf 'Scene\t%s\t%s\n' "$after" "$before"
    done | awk -F '\t' -v min_change=0.03 -v min_agree=6 -f "$awk_script" | grep '^Scene'
}

expect() {
    local verdict=$1 out
    shift
    out=$(summary "$@")
    if [[ "$out" != *"$verdict" ]]; then
        echo "FAIL: expected '$verdict' for loads $*; got: $out"
        failures=$((failures + 1))
    fi
}

# Same loads every round: no change.
expect "no clear change" 1 1 1 1 1 1 1 1 1 1 1 1 1 1
# 10% slower every round.
expect "slower" 1.1 1 1.1 1 1.1 1 1.1 1 1.1 1 1.1 1 1.1 1
# 10% faster every round.
expect "faster" 0.9 1 0.9 1 0.9 1 0.9 1 0.9 1 0.9 1 0.9 1
# 10% slower in five rounds, faster in two: too few agree.
expect "no clear change" 1.1 1 1.1 1 1.1 1 1.1 1 1.1 1 0.9 1 0.9 1
# 10% slower in six rounds of seven: enough agree.
expect "slower" 1.1 1 1.1 1 1.1 1 1.1 1 1.1 1 1.1 1 0.9 1
# 2% slower every round: below the threshold.
expect "no clear change" 1.02 1 1.02 1 1.02 1 1.02 1 1.02 1 1.02 1 1.02 1

# The line shows the median ratio and its range.
out=$(summary 1.2 1 1.1 1 1.3 1 1.1 1 1.1 1 1.4 1 1.2 1)
if [[ "$out" != *"1.200"*"1.100"*"1.400"* ]]; then
    echo "FAIL: expected median 1.200, range 1.100 to 1.400; got: $out"
    failures=$((failures + 1))
fi

# Scenes are summarised separately, in the order they first appear.
out=$(printf 'B\t2\t1\nA\t1\t1\nB\t2\t1\nA\t1\t1\n' | awk -F '\t' -v min_change=0.03 -v min_agree=2 -f "$awk_script" | grep -E '^(A|B) ')
if [ "$(printf '%s\n' "$out" | cut -c1)" != "$(printf 'B\nA')" ] || [[ "$out" != B*slower*A*"no clear change" ]]; then
    echo "FAIL: expected B slower then A no clear change; got: $out"
    failures=$((failures + 1))
fi

[ "$failures" = 0 ] && echo "CPU A/B verdicts: all cases pass"
exit "$failures"
