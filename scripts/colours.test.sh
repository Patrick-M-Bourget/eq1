#!/usr/bin/env bash
# Tests the colours check (scripts/check.sh colours) against scratch files in a throwaway git repo holding a copy of
# check.sh: each hard-coded colour spelling under plugin/ must fail with its file and line, and the same spelling in
# plugin/staple/, or a colour that isn't a literal, must pass.
# Run: scripts/check.sh hooks (by hand, or in a full check; never from a git hook).
set -uo pipefail
check=$(cd "$(dirname "$0")" && pwd)/check.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
failures=0

# A repo with one tracked file, plugin/<path>, holding the line; prints the check's output and returns its status.
run_check() {
    local path=$1 line=$2 repo
    repo=$(mktemp -d "$tmp/repo.XXXXXX")
    mkdir -p "$repo/scripts" "$repo/$(dirname "plugin/$path")"
    cp "$check" "$repo/scripts/check.sh"
    printf '// scratch\n%s\n' "$line" > "$repo/plugin/$path"
    git -C "$repo" init -q
    git -C "$repo" add -A
    "$repo/scripts/check.sh" colours 2>&1
}

fails() {
    local line=$1 out
    if out=$(run_check editor/Scratch.cpp "$line"); then
        echo "FAIL: passed on hard-coded colour: $line"
        failures=$((failures + 1))
    elif ! grep -q 'plugin/editor/Scratch.cpp:2:' <<< "$out"; then
        echo "FAIL: no file and line for: $line"
        printf '%s\n' "$out"
        failures=$((failures + 1))
    fi
}

passes() {
    local path=$1 line=$2 out
    if ! out=$(run_check "$path" "$line"); then
        echo "FAIL: flagged in plugin/$path: $line"
        printf '%s\n' "$out"
        failures=$((failures + 1))
    fi
}

fails 'auto c = juce::Colour (0xff102030);'
fails 'auto c = juce::Colour (0XFF102030);'
fails 'auto c = juce::Colour { 0xff102030 };'
fails 'auto c = juce::Colour (255, 255, 255);'
fails 'auto c = juce::Colour (0.5f, 0.5f, 0.5f, 1.0f);'
fails 'auto c = juce::Colour::fromFloatRGBA (0.5f, 0.5f, 0.5f, 1.0f);'
fails 'auto c = juce::Colour::fromString ("ff102030");'
fails 'auto c = juce::Colour::fromRGB (1, 2, 3);'
fails 'auto c = juce::Colour::fromRGBA (1, 2, 3, 4);'
fails 'auto c = juce::Colour::fromHSV (0.5f, 0.5f, 0.5f, 1.0f);'
fails 'auto c = juce::Colour::fromHSL (0.5f, 0.5f, 0.5f, 1.0f);'
fails 'auto c = juce::Colour::greyLevel (0.5f);'
fails 'g.setColour (juce::Colours::white);'
fails 'g.fillAll (juce::Colours::transparentBlack);'

passes staple/Tokens.h 'inline const juce::Colour ink { 0xff102030 };'
passes staple/Tokens.h 'inline const juce::Colour grey = juce::Colour::greyLevel (0.5f);'
passes editor/Scratch.cpp 'g.setColour (staple::ink.withAlpha (0.5f));'
passes editor/Scratch.cpp 'setColour (0x1000280, staple::ink);'
passes editor/Scratch.cpp 'juce::Colour c;'

[ "$failures" = 0 ] && echo "Colours check: every hard-coded spelling caught, tokens and non-literals pass"
exit $((failures > 0))
