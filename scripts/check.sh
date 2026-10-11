#!/usr/bin/env bash
# Runs the checks CI runs (.github/workflows/ci.yml calls this script, a stage at a time), and the local-only
# paint time, on macOS or Windows (Git Bash).
#
#   scripts/check.sh            docs, hooks, build, test, cpu, paint, tsan and validate
#   scripts/check.sh docs       every doc section cited in code (docs/<file>.md, "<Section>") exists, GLOSSARY.md is the only
#                               glossary, no test reads a saved state as raw bytes, runs timers itself, has a non-ASCII
#                               title, reads an EQ1_ environment variable or opens a Graphics on an image outside
#                               tests/plugin/EditorHarness.h, and no colour is hard-coded in plugin/ outside plugin/staple/
#   scripts/check.sh colours    only that last check: no colour is hard-coded in plugin/ outside plugin/staple/
#   scripts/check.sh hooks      the Claude Code worktree hook's, merge-on-green.sh's and the colours check's tests,
#                               against fakes (not run by git hooks, which run docs)
#   scripts/check.sh build      configure and build every format (macOS Universal / Windows x64),
#                               without link-time optimisation (EQ1_LTO=OFF; shipping builds keep its default, ON)
#   scripts/check.sh test       Engine and Plugin Shell tests
#   scripts/check.sh focus <re> build the tests and run those whose names match the regex; none matching fails
#   scripts/check.sh screens <dir>  build the tests and render every hidden [.screens] test into <dir> as PNGs, to
#                               compare with the rendered Staple prototype
#   scripts/check.sh cpu        the Engine's CPU load against its budget (docs/performance.md, "CPU budget"); on a
#                               Mac busy with other work it measures nothing and exits 3
#   scripts/check.sh paint      the editor's paint time against its ceiling (docs/performance.md, "Paint time"); local
#                               only, not in CI; on a Mac busy with other work it measures nothing and exits 3
#   scripts/check.sh tsan       Engine tests under ThreadSanitizer (macOS only)
#   scripts/check.sh validate   pluginval (VST3, AU) at every sample rate eq1 supports, auval, Sidechain
#                               routing (VST3, AU), clap-validator, AAX and Standalone built
#
# BUILD_DIR (default build) and FETCHCONTENT_BASE_DIR (default .deps, or the main checkout's .deps in a linked
# worktree) can be overridden; CMake's
# CMAKE_C_COMPILER_LAUNCHER and CMAKE_CXX_COMPILER_LAUNCHER environment variables (sccache in CI) apply. Validators
# are downloaded into the dependencies folder with gh, which needs to be authenticated (GH_TOKEN in CI).
set -euo pipefail
# Where it was called from, for a path given relative to it (screens <dir>).
CALLER_DIR=$PWD
cd "$(dirname "$0")/.."

BUILD_DIR=${BUILD_DIR:-build}
# A linked worktree shares the main checkout's dependencies rather than fetching its own over a slow link.
common_dir=$(git rev-parse --path-format=absolute --git-common-dir 2> /dev/null || true)
main_checkout=${common_dir%/.git}
if [ -z "${FETCHCONTENT_BASE_DIR:-}" ] && [ -n "$common_dir" ] && [ "$main_checkout" != "$PWD" ] && [ -d "$main_checkout/.deps" ]; then
    DEPS=$main_checkout/.deps
else
    DEPS=${FETCHCONTENT_BASE_DIR:-$PWD/.deps}
fi
PLUGINVAL_VERSION=v1.0.4
CLAP_VALIDATOR_VERSION=0.4.1
ARTEFACTS=$BUILD_DIR/plugin/eq1_artefacts/Release
ROUTING_CHECK=$BUILD_DIR/tests/eq1_sidechain_routing_check_artefacts/Release/eq1_sidechain_routing_check

case "$(uname -s)" in
    Darwin) os=macos ;;
    MINGW* | MSYS* | CYGWIN*) os=windows; DEPS=$(cygpath -m "$DEPS") ;;
    *) echo "eq1 builds on macOS and Windows only" >&2; exit 1 ;;
esac

step() { printf '\n== %s\n' "$*"; }

# A citation in code, docs/<file>.md, "<Section>", must name a doc that exists and a heading or bold
# label (**<Section>** or **<Section>:**) in it; a bare docs/<file>.md must name a doc that exists.
docs() {
    step "Doc references"
    local broken=0 ref file section
    while IFS= read -r ref; do
        file=${ref%%,*}
        if [ ! -f "$file" ]; then
            echo "$ref: no such file" >&2
            broken=1
            continue
        fi
        [ "$ref" = "$file" ] && continue
        section=${ref#*, \"}
        section=${section%\"}
        if ! awk -v s="$section" '{ h = $0; sub (/^#+ /, "", h) } ($0 ~ /^#+ / && h == s) || index ($0, "**" s "**") || index ($0, "**" s ":**") { found = 1 } END { exit ! found }' "$file"; then
            echo "$ref: no such section" >&2
            broken=1
        fi
    done < <(git ls-files -z -- ':!*.md' | xargs -0 grep -hoE 'docs/[A-Za-z0-9_./-]+\.md(, "[^"]+")?' | sort -u)
    # One glossary: GLOSSARY.md at the root (or the ones GLOSSARY-MAP.md lists). A copy elsewhere,
    # such as one bundled with a design handoff, goes stale.
    if [ ! -f GLOSSARY-MAP.md ]; then
        while IFS= read -r copy; do
            echo "$copy: a second glossary; GLOSSARY.md at the root is the only one" >&2
            broken=1
        done < <(git ls-files -- '*GLOSSARY.md' ':!GLOSSARY.md')
    fi
    # A saved state starts with a binary header, so its raw bytes never show the XML: a test that
    # searches them passes whatever was saved. Tests decode it with tests/plugin/SavedState.h.
    local raw
    raw=$(git grep -nE '[A-Za-z_]*[sS]tate\.toString *\(\)' -- tests || true)
    if [ -n "$raw" ]; then
        printf '%s\n' "$raw" | sed 's/$/: a saved state read as raw bytes; decode it with eq1::test::savedState (tests\/plugin\/SavedState.h)/' >&2
        broken=1
    fi
    # A test waits for the editor's timers with harness::settle (tests/plugin/EditorHarness.h): a loop of
    # its own waits wall-clock time, which ends before the timers run on a loaded CI runner.
    local loops
    loops=$(git grep -n 'callPendingTimersSynchronously' -- tests ':!tests/plugin/EditorHarness.h' || true)
    if [ -n "$loops" ]; then
        printf '%s\n' "$loops" | sed 's/$/: runs timers itself; wait with harness::settle (tests\/plugin\/EditorHarness.h)/' >&2
        broken=1
    fi
    # ctest hands a test's name to the test executable in the machine's code page, which on Windows
    # can't hold "…" or "–": a title with them matches no test there. Titles stay ASCII.
    local titles
    titles=$(LC_ALL=C git grep -nE '(TEST_CASE|SECTION) *\("[^"]*[^ -~]' -- tests || true)
    if [ -n "$titles" ]; then
        printf '%s\n' "$titles" | sed 's/$/: a test title outside ASCII; ctest on Windows runs no test by that name/' >&2
        broken=1
    fi
    # Renders go through harness::writeSnapshot, the one reader of its environment variable, so a test
    # doesn't grow plumbing of its own for them again.
    local variables
    variables=$(git grep -nE '(getenv|getEnvironmentVariable) *\( *"EQ1_' -- tests ':!tests/plugin/EditorHarness.h' || true)
    if [ -n "$variables" ]; then
        printf '%s\n' "$variables" | sed 's/$/: reads an EQ1_ environment variable; write renders with harness::writeSnapshot (tests\/plugin\/EditorHarness.h)/' >&2
        broken=1
    fi
    # A pixel read while an image's Graphics is open sees nothing on Windows (Direct2D draws when the
    # context ends): tests draw on images through harness::paintImage, which closes it first.
    local graphics
    graphics=$(git grep -nE 'Graphics +[A-Za-z_]+ *[({] *[A-Za-z_]' -- tests ':!tests/plugin/EditorHarness.h' || true)
    if [ -n "$graphics" ]; then
        printf '%s\n' "$graphics" | sed 's/$/: opens a Graphics on an image; draw with harness::paintImage (tests\/plugin\/EditorHarness.h)/' >&2
        broken=1
    fi
    [ "$broken" = 0 ] && echo "Every cited doc and section exists, no test reads a saved state as raw bytes or an EQ1_ environment variable or opens a Graphics on an image, tests wait with harness::settle and have ASCII titles"
    return "$broken"
}

# The worktree hook (.claude/hooks/one-branch-per-worktree.sh) against its cases.
hooks() {
    step "Worktree hook"
    .claude/hooks/one-branch-per-worktree.test.sh
    step "Merge on green"
    scripts/merge-on-green.test.sh
    step "Colours check"
    scripts/colours.test.sh
}

# Every colour in the editor comes from Staple's tokens (plugin/staple/Tokens.h): a colour written as a
# number (Colour (0xff..), Colour (255, ..), Colour (0.5f, ..), any case of 0x), made by a Colour:: factory
# (fromRGB, fromFloatRGBA, fromString, greyLevel, ..), or a named juce::Colours one, belongs in plugin/staple/
# only. The spellings caught are in scripts/colours.test.sh.
colours() {
    step "Colours from tokens"
    local found
    found=$(git ls-files -z -- plugin ':!plugin/staple/' |
        xargs -0 grep -HnE '(^|[^A-Za-z0-9_])Colour *[({] *[0-9.]|Colour::(from[A-Za-z]+|greyLevel) *\(|Colours::' || true)
    if [ -n "$found" ]; then
        printf '%s\n' "$found" | sed 's/$/  <- hard-coded colour: use a token from plugin\/staple\/Tokens.h/' >&2
        return 1
    fi
    echo "Every colour in plugin/ outside plugin/staple/ comes from a token"
}

# A CMake build folder keeps the generator it was made with, and refuses another. The dependencies'
# own build folders (FetchContent subbuilds, juceaide) are caches: one made with another generator
# (a restored CI cache, or a switch between Ninja and Visual Studio) is deleted and rebuilt. The main
# build folder isn't deleted unasked.
match_generator() {
    local generator=$1 cache made
    for cache in "$DEPS"/*/CMakeCache.txt "$DEPS"/*/*/CMakeCache.txt; do
        [ -f "$cache" ] || continue
        made=$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$cache")
        if [ -n "$made" ] && [ "${made#"$generator"}" = "$made" ]; then
            echo "$(dirname "$cache") was made with $made, not $generator: deleting it to rebuild"
            rm -rf "$(dirname "$cache")"
        fi
    done
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        made=$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$BUILD_DIR/CMakeCache.txt")
        if [ "${made#"$generator"}" = "$made" ]; then
            echo "$BUILD_DIR was made with $made, not $generator: delete it, or set BUILD_DIR, and run again" >&2
            exit 1
        fi
    fi
}

build() {
    step "Build ($os)"
    if [ "$os" = macos ]; then
        match_generator Ninja
        cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
            -DEQ1_LTO=OFF "-DFETCHCONTENT_BASE_DIR=$DEPS"
    elif command -v cl > /dev/null; then
        # In an MSVC developer environment (CI sets one up), Ninja compiles every file in parallel.
        match_generator Ninja
        cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DEQ1_LTO=OFF "-DFETCHCONTENT_BASE_DIR=$DEPS"
    else
        # Without one, Visual Studio finds the compiler itself.
        match_generator "Visual Studio"
        cmake -S . -B "$BUILD_DIR" -A x64 -DEQ1_LTO=OFF "-DFETCHCONTENT_BASE_DIR=$DEPS"
    fi
    cmake --build "$BUILD_DIR" --config Release --parallel
}

# In parallel, as the pre-commit hook runs them: the CPU budget, which needs the machine to itself,
# is its own step.
run_tests() {
    step "Engine and Plugin Shell tests"
    ctest --test-dir "$BUILD_DIR" -C Release -j 8 --output-on-failure
}

# A test filter that matches nothing is an error here, not a silent pass. Its output goes to a log, as
# the hooks' does: the summary on success; the compiler's errors, or the failing tests' output, on failure.
focus() {
    step "Tests matching $1"
    local log=$BUILD_DIR/focus.log
    if ! cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_engine_tests eq1_plugin_tests > "$log" 2>&1; then
        grep -E 'error|FAILED:' "$log" >&2
        echo "Build failed; log in $log" >&2
        return 1
    fi
    if ! ctest --test-dir "$BUILD_DIR" -C Release -R "$1" --no-tests=error -j 8 --output-on-failure >> "$log" 2>&1; then
        sed -n '/^Test project/,$p' "$log" | grep -vE '^ +Start +[0-9]+:|Test +#[0-9]+: .* Passed' >&2
        echo "Log in $log" >&2
        return 1
    fi
    grep -E '^[0-9]+% tests passed' "$log"
}

# Every hidden [.screens] test's renders, written into one folder by harness::writeSnapshot.
# A tag matching no test fails rather than writing nothing.
screens() {
    local dir=$1
    case "$dir" in
        /* | [A-Za-z]:*) ;;
        *) dir=$CALLER_DIR/$dir ;;
    esac
    step "Renders into $dir"
    mkdir -p "$dir"
    dir=$(cd "$dir" && pwd)
    cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_plugin_tests
    EQ1_SCREENS=$dir "$(test_exe eq1_plugin_tests)" --warn UnmatchedTestSpec "[.screens]"
    ls "$dir"
}

# Timings taken while the machine is busy (other builds, other agents) measure the machine, not eq1:
# a timing stage refuses, saying why, rather than report a false overrun. CI's runners are quiet, so CI
# always measures.
busy() {
    [ "$os" = macos ] && [ -z "${GITHUB_ACTIONS:-}" ] || return 1
    local load cores
    load=$(sysctl -n vm.loadavg | awk '{ print $2 }')
    cores=$(sysctl -n hw.ncpu)
    awk -v l="$load" -v c="$cores" 'BEGIN { exit ! (l > c / 2) }' || return 1
    echo "Machine busy (load $load on $cores cores): $1" >&2
}

# A test executable built in BUILD_DIR/tests (in its Release folder with Visual Studio).
test_exe() {
    local exe=$BUILD_DIR/tests/$1
    [ "$os" = windows ] && [ ! -f "$exe.exe" ] && exe=$BUILD_DIR/tests/Release/$1
    printf '%s\n' "$exe"
}

# On its own, after the tests: timings taken while anything else runs are meaningless.
cpu() {
    step "CPU budget"
    busy "CPU budget not measured; CI measures it on every PR" && return 3
    # `all` calls cpu under ||, where set -e is off: return a failed build rather than time a stale exe.
    cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_cpu_budget || return
    local exe out status=0
    exe=$(test_exe eq1_cpu_budget)
    out=$("$exe") || status=$?
    printf '%s\n' "$out"
    # In CI the numbers also go to the job summary and as annotations: once the job ends, one API call
    # (check-runs/<job>/annotations) reads them without downloading the log.
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
        printf '### CPU budget (%s)\n\n```\n%s\n```\n' "$os" "$out" >> "$GITHUB_STEP_SUMMARY"
    fi
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        grep 'kHz' <<< "$out" | while IFS= read -r line; do echo "::notice title=CPU budget ($os)::$line"; done
    fi
    return "$status"
}

# The editor drawing a busy frame at 2x, from the hidden [paint] test, which fails above its ceiling.
# Local only: CI has no step for it.
paint() {
    step "Paint time"
    busy "paint time not measured" && return 3
    # `all` calls paint under ||, where set -e is off: return a failed build rather than time a stale exe.
    cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_plugin_tests || return
    "$(test_exe eq1_plugin_tests)" "[paint]"
}

tsan() {
    if [ "$os" != macos ]; then
        echo "ThreadSanitizer runs on macOS only; skipped"
        return
    fi
    # Catches data races in the lock-free settings handoff and the analysis taps. The frequency
    # response grids ([response]) and the sweeps across sample rates and settings ([sweep]) run
    # single-threaded, so they are left to the normal run.
    step "Engine tests under ThreadSanitizer"
    cmake -S . -B "$BUILD_DIR-tsan" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEQ1_BUILD_PLUGIN=OFF \
        "-DFETCHCONTENT_BASE_DIR=$DEPS" -DCMAKE_CXX_FLAGS=-fsanitize=thread -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
    cmake --build "$BUILD_DIR-tsan" --parallel
    TSAN_OPTIONS=halt_on_error=1 ctest --test-dir "$BUILD_DIR-tsan" --output-on-failure -LE 'response|sweep'
}

fetch_validators() {
    local dir="$DEPS/validators/$os-$PLUGINVAL_VERSION-$CLAP_VALIDATOR_VERSION"
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir.partial"
        (
            cd "$dir.partial"
            if [ "$os" = macos ]; then
                gh release download "$PLUGINVAL_VERSION" -R Tracktion/pluginval -p pluginval_macOS.zip
                gh release download "$CLAP_VALIDATOR_VERSION" -R free-audio/clap-validator -p '*macos-universal.zip'
                unzip -q pluginval_macOS.zip
                unzip -q ./*macos-universal.zip && tar xzf ./*.tar.gz
            else
                gh release download "$PLUGINVAL_VERSION" -R Tracktion/pluginval -p pluginval_Windows.zip
                gh release download "$CLAP_VALIDATOR_VERSION" -R free-audio/clap-validator -p '*windows.zip'
                unzip -q pluginval_Windows.zip
                unzip -q ./*windows.zip
            fi
        )
        mv "$dir.partial" "$dir"
    fi
    if [ "$os" = macos ]; then
        PLUGINVAL="$dir/pluginval.app/Contents/MacOS/pluginval"
        CLAP_VALIDATOR="$dir/binaries/clap-validator"
    else
        PLUGINVAL="$dir/pluginval.exe"
        CLAP_VALIDATOR="$dir/clap-validator.exe"
    fi
}

# Every sample rate eq1 supports, and block sizes from a sample at a time to larger than most hosts use.
pluginval() {
    "$PLUGINVAL" --strictness-level 10 --sample-rates 44100,48000,88200,96000,176400,192000 \
        --block-sizes 1,7,64,128,256,512,1024,4096 --validate-in-process --validate "$1"
}

# macOS only finds an AU once it is installed and registered: install it for the check, and remove
# it on the way out whether or not the check passes (a subshell, so the EXIT trap stays local).
validate_au() (
    components="$HOME/Library/Audio/Plug-Ins/Components"
    if [ -e "$components/eq1.component" ]; then
        echo "$components/eq1.component already exists; remove it to validate the AU" >&2
        exit 1
    fi
    mkdir -p "$components"
    trap 'rm -rf "$components/eq1.component"; killall -9 AudioComponentRegistrar 2>/dev/null || true' EXIT
    cp -R "$ARTEFACTS/AU/eq1.component" "$components/"
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    auval -v aufx Eq01 Pmbg
    pluginval "$components/eq1.component"
    "$ROUTING_CHECK" AU
)

validate() {
    fetch_validators
    step "pluginval VST3"
    pluginval "$ARTEFACTS/VST3/eq1.vst3"
    "$ROUTING_CHECK" "$ARTEFACTS/VST3/eq1.vst3"
    if [ "$os" = macos ]; then
        step "auval and pluginval AU"
        validate_au
    fi
    step "clap-validator CLAP"
    "$CLAP_VALIDATOR" validate --only-failed "$ARTEFACTS/CLAP/eq1.clap"
    # AAX can only be hosted by Pro Tools, and Avid's validator needs a developer account (#16),
    # so the unsigned AAX build is only checked to exist.
    step "AAX and Standalone built"
    test -d "$ARTEFACTS/AAX/eq1.aaxplugin"
    if [ "$os" = macos ]; then test -d "$ARTEFACTS/Standalone/eq1.app"; else test -f "$ARTEFACTS/Standalone/eq1.exe"; fi
    echo "AAX and Standalone present"
}

case "${1:-all}" in
    build) build ;;
    test) run_tests ;;
    cpu) cpu ;;
    paint) paint ;;
    focus) focus "${2:?usage: scripts/check.sh focus <regex>}" ;;
    screens) screens "${2:?usage: scripts/check.sh screens <dir>}" ;;
    tsan) tsan ;;
    validate) validate ;;
    docs) docs; colours ;;
    colours) colours ;;
    hooks) hooks ;;
    # A busy machine skips the CPU budget and the paint time (exit 3) but not the stages after them.
    all) docs; colours; hooks; build; run_tests; cpu || [ $? -eq 3 ]; paint || [ $? -eq 3 ]; tsan; validate ;;
    *) sed -n '2,24p' "$0" >&2; exit 2 ;;
esac
