#!/usr/bin/env bash
# Runs the checks CI runs (.github/workflows/ci.yml calls this script, a stage at a time), and the local-only
# paint time, on macOS or Windows (Git Bash).
#
#   scripts/check.sh            docs, hooks, build, test, cpu, paint, tsan and validate (not null, which needs a ref)
#   scripts/check.sh docs       every doc section cited in code (docs/<file>.md, "<Section>") exists, GLOSSARY.md is the only
#                               glossary, no two ADRs share a number, the filter lab's parameter ranges match the
#                               plugin's, code points at no closed ticket (when gh is signed in), no test reads a saved state as raw bytes, runs
#                               timers itself, has a non-ASCII title, reads an EQ1_ environment variable or opens a
#                               Graphics on an image outside
#                               tests/plugin/EditorHarness.h, and no colour is hard-coded in plugin/ outside plugin/staple/
#   scripts/check.sh colours    only that last check: no colour is hard-coded in plugin/ outside plugin/staple/
#   scripts/check.sh hooks      the Claude Code worktree hook's, merge-on-green.sh's, the git pre-push hook's, the
#                               colours check's, the CPU A/B verdicts' and the ticket-citation rule's tests, against fakes and scratch repos (not run by git hooks, which run
#                               docs)
#   scripts/check.sh build      configure and build every format (macOS Universal / Windows x64),
#                               without link-time optimisation (EQ1_LTO=OFF; shipping builds keep its default, ON)
#   scripts/check.sh test       Engine and Plugin Shell tests
#   scripts/check.sh focus <re> build the tests and run those whose names match the regex; none matching fails
#   scripts/check.sh screens <dir>  build the tests and render every hidden [.screens] test into <dir> as PNGs, to
#                               compare with the rendered Staple prototype
#   scripts/check.sh cpu        the Engine's CPU load in every CPU scene against its budget (docs/performance.md,
#                               "CPU budget"); on a Mac busy with other work it measures nothing and exits 3
#   scripts/check.sh cpu <ref>  the CPU A/B (docs/performance.md, "CPU A/B"): builds <ref>'s Engine with this tree's
#                               CPU scenes in a temporary worktree, runs each scene through both builds in turn for 7
#                               rounds, and prints per scene the median after/before load ratio (above 1: this tree
#                               is slower), its min and max, and "slower", "faster" or "no clear change". Runs on a
#                               busy machine too, printing its load; a report: exits 0 whatever the verdicts
#   scripts/check.sh null <ref> the null check (docs/performance.md, "Null check"): builds <ref>'s render tool in a
#                               temporary worktree (removed on exit) and this tree's, renders every render scene through
#                               both Engines into BUILD_DIR/null, and prints each scene's worst deviation in dBFS, or
#                               "exact"; fails above -120 dBFS, or on a scene <ref> has that this tree hasn't
#   scripts/check.sh paint      the editor's paint time against its ceiling (docs/performance.md, "Paint time"); local
#                               only, not in CI; on a Mac busy with other work it measures nothing and exits 3
#   scripts/check.sh size       the shipping build's footprint (docs/performance.md, "Footprint"): each format's
#                               binary per architecture, as built and after strip -x; that the .dSYM beside each
#                               bundle carries the binary's UUIDs and resolves its entry point (fails otherwise);
#                               the VST3's code by library; and the heap one Engine holds, whose test fails above
#                               its ceiling. Not in CI. On Windows, the heap only
#   scripts/check.sh tsan       Engine tests under ThreadSanitizer (macOS only)
#   scripts/check.sh asan       Engine and Plugin Shell tests under AddressSanitizer and UndefinedBehaviorSanitizer
#                               (macOS only; not in the default run); where the toolchain's AddressSanitizer
#                               hangs at startup, it says so and runs UndefinedBehaviorSanitizer alone; with
#                               EQ1_REQUIRE_ASAN=1 (for a CI runner whose ASan works) it fails there instead
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
    # Each ADR has its own number. Two parallel branches can each take the next one; the second to
    # rebase onto main finds out here.
    local taken
    taken=$(git ls-files -- 'docs/adr/[0-9]*.md' | sed -E 's|docs/adr/([0-9]+)-.*|\1|' | sort | uniq -d)
    if [ -n "$taken" ]; then
        printf '%s\n' "$taken" | sed 's|^|docs/adr/|; s|$|-*: two ADRs share this number; renumber the newer one|' >&2
        broken=1
    fi
    # A saved state starts with a binary header, so its raw bytes never show the XML: a test that
    # searches them passes whatever was saved. Tests decode it with harness::savedState (tests/plugin/EditorHarness.h). Flagged:
    # toString on any block a file fills with getStateInformation, whatever the block is called.
    local raw file block
    raw=$(git grep -lE 'get(CurrentProgram)?StateInformation *\(' -- tests | while IFS= read -r file; do
        grep -oE 'get(CurrentProgram)?StateInformation *\( *[A-Za-z_][A-Za-z0-9_]*' "$file" | sed -E 's/.*\( *//' | sort -u |
            while IFS= read -r block; do
                grep -nE "(^|[^A-Za-z0-9_.])$block *\. *toString *\(" "$file" | sed "s|^|$file:|" || true
            done
    done || true)
    if [ -n "$raw" ]; then
        printf '%s\n' "$raw" | sed 's/$/: a saved state read as raw bytes; decode it with harness::savedState (tests\/plugin\/EditorHarness.h)/' >&2
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
    # The filter lab measures over a Band's parameter ranges; a copy that drifts from the plugin's
    # leaves part of the range unmeasured while the lab reports it covered. The plugin's are named once,
    # in the Engine's settings header.
    local name lab plugin
    for name in frequency gain q slope; do
        lab=$(grep -E "^$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')_RANGE = \(" tools/filter-lab/filterlab.py |
            sed -E 's/.*\(([^,]+), *([^)]+)\).*/\1 \2/')
        plugin=$(grep -E "^inline constexpr ParameterRange $name \{" engine/include/eq1/Settings.h |
            sed -E 's/.*\{ *([^,]+), *([^ }]+) *\}.*/\1 \2/')
        if [ -z "$lab" ] || [ -z "$plugin" ] || ! awk -v a="$lab" -v b="$plugin" 'BEGIN { split (a, x, " "); split (b, y, " "); exit ! (x[1] == y[1] && x[2] == y[2]) }'; then
            echo "tools/filter-lab/filterlab.py: ${name}'s range reads '${lab}', engine/include/eq1/Settings.h '${plugin}'; keep them equal" >&2
            broken=1
        fi
    done
    # Code points only at open tickets (CODING_STANDARDS.md, "Comments and docs state current behaviour"):
    # a closed one is history, which belongs in commit messages. It asks GitHub which are closed, so it
    # is skipped where gh isn't signed in (CI's docs stage among them).
    local closed stale
    if closed=$(gh issue list --state closed --limit 2000 --json number --jq '.[].number' 2>/dev/null) && [ -n "$closed" ]; then
        # shellcheck disable=SC2086 # one ticket number per word
        stale=$(scripts/cited-tickets.sh $closed | sed -E 's/:(#[0-9]+)$/: points at closed ticket \1/' || true)
        if [ -n "$stale" ]; then
            printf '%s\n' "$stale" | sed 's/$/; say what the code does now, or point at an open ticket/' >&2
            broken=1
        else
            echo "Code points at no closed ticket"
        fi
    else
        echo "Skipped the closed-ticket check: gh isn't signed in"
    fi
    [ "$broken" = 0 ] && echo "Every cited doc and section exists, every ADR has its own number, no test reads a saved state as raw bytes or an EQ1_ environment variable or opens a Graphics on an image, tests wait with harness::settle and have ASCII titles, the filter lab's parameter ranges match the plugin's"
    return "$broken"
}

# The worktree hook (.claude/hooks/one-branch-per-worktree.sh), scripts/merge-on-green.sh and the pre-push
# hook (.githooks/pre-push) against their cases.
hooks() {
    step "Worktree hook"
    .claude/hooks/one-branch-per-worktree.test.sh
    step "Merge on green"
    scripts/merge-on-green.test.sh
    step "Pre-push hook"
    scripts/pre-push.test.sh
    step "Colours check"
    scripts/colours.test.sh
    step "CPU A/B verdicts"
    scripts/cpu-ab.test.sh
    step "Ticket citations"
    scripts/cited-tickets.test.sh
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

# Configures a source tree into a build folder the way CI builds: Release, without link-time
# optimisation or stripping (its binaries keep their symbols), sharing the dependencies. The main build
# is build(); the null check configures <ref>'s.
configure() {
    local source=$1 build=$2
    shift 2
    if [ "$os" = macos ]; then
        match_generator Ninja
        cmake -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
            -DEQ1_LTO=OFF -DEQ1_STRIP=OFF "-DFETCHCONTENT_BASE_DIR=$DEPS" "$@"
    elif command -v cl > /dev/null; then
        # In an MSVC developer environment (CI sets one up), Ninja compiles every file in parallel.
        match_generator Ninja
        cmake -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DEQ1_LTO=OFF -DEQ1_STRIP=OFF \
            "-DFETCHCONTENT_BASE_DIR=$DEPS" "$@"
    else
        # Without one, Visual Studio finds the compiler itself.
        match_generator "Visual Studio"
        cmake -S "$source" -B "$build" -A x64 -DEQ1_LTO=OFF -DEQ1_STRIP=OFF "-DFETCHCONTENT_BASE_DIR=$DEPS" "$@"
    fi
}

build() {
    step "Build ($os)"
    configure . "$BUILD_DIR"
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

# <ref> checked out in a temporary worktree, work/src, and configured into work/build the way this tree is
# built (same compiler and flags) with the Engine and the test tools only, no Plugin Shell, so a difference
# between the two builds is the source's. Called in a subshell (null, cpu_ab): it sets an EXIT trap that
# removes the worktree and its build. With a function name as third argument, that function is called
# with the worktree's path before configuring, to change its sources.
ref_tree() {
    local ref=$1 work=$2 prepare=${3:-}
    if ! git rev-parse --verify --quiet "$ref^{commit}" > /dev/null; then
        echo "no such ref: $ref" >&2
        exit 1
    fi
    git worktree remove --force "$work/src" 2> /dev/null || rm -rf "$work/src"
    # shellcheck disable=SC2064 # work is expanded now, on purpose
    trap "git worktree remove --force '$work/src' 2> /dev/null || true; rm -rf '$work/src' '$work/build'" EXIT
    git worktree add --quiet --detach "$work/src" "$ref"
    [ -z "$prepare" ] || "$prepare" "$work/src"
    configure "$work/src" "$work/build" -DEQ1_BUILD_PLUGIN=OFF > "$work/configure.log" 2>&1 || { cat "$work/configure.log" >&2; exit 1; }
}

# Both trees' render tools (tests/render/RenderTool.cpp) render the same scenes; this tree's compares
# them. <ref> is built by ref_tree under BUILD_DIR/null.
null() (
    local ref=$1 work=$BUILD_DIR/null
    step "Null check against $ref"
    mkdir -p "$work"
    work=$(cd "$work" && pwd)
    rm -rf "$work/current" "$work/ref"
    ref_tree "$ref" "$work" null_needs_render_tool
    cmake --build "$work/build" --config Release --parallel --target eq1_render
    cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_render
    "$(BUILD_DIR=$work/build test_exe eq1_render)" render "$work/ref"
    "$(test_exe eq1_render)" render "$work/current"
    "$(test_exe eq1_render)" compare "$work/current" "$work/ref"
)

null_needs_render_tool() {
    if [ ! -f "$1/tests/render/RenderTool.cpp" ]; then
        echo "null: the ref has no render tool (tests/render/RenderTool.cpp); compare against a ref that has one" >&2
        exit 1
    fi
}

# The CPU A/B (docs/performance.md, "CPU A/B"): this tree's CPU budget executable against <ref>'s, scene by
# scene, the two alternating so that a busy machine's load hits both alike. A report, not a gate: it runs
# whatever the load and exits 0 whatever the verdicts.
cpu_ab_rounds=7          # rounds; each runs every scene once on each side, the side going first swapped every round
cpu_ab_seconds=1         # wall-clock seconds each run is timed for
cpu_ab_rate=48000        # sample rate, Hz
cpu_ab_min_change=0.03   # a verdict needs the median after/before ratio at least this far from 1 ...
cpu_ab_min_agree=6       # ... and at least this many rounds going the same way

# <ref> is built by ref_tree under BUILD_DIR/cpu-ab with this tree's CPU scenes (tests/cpu/ and
# tests/engine/Measure.h copied over its own), so both sides run the same scenes and only the Engine differs.
cpu_ab() (
    local ref=$1 work=$BUILD_DIR/cpu-ab
    step "CPU A/B against $ref"
    mkdir -p "$work"
    work=$(cd "$work" && pwd)
    ref_tree "$ref" "$work" cpu_ab_scenes
    cmake --build "$work/build" --config Release --parallel --target eq1_cpu_budget > "$work/build.log" 2>&1 || {
        grep -E 'error|FAILED:' "$work/build.log" >&2
        echo "cpu: this tree's CPU scenes don't build against $ref's Engine; log in $work/build.log" >&2
        exit 1
    }
    cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_cpu_budget
    local after before scenes round scene name first second a b
    after=$(test_exe eq1_cpu_budget)
    before=$(BUILD_DIR=$work/build test_exe eq1_cpu_budget)
    scenes=$("$after" scenes | tr -d '\r')
    echo "Load before: $(load_average)"
    : > "$work/loads.tsv"
    for round in $(seq "$cpu_ab_rounds"); do
        scene=0
        while IFS= read -r name; do
            if [ $((round % 2)) = 1 ]; then first=$after second=$before; else first=$before second=$after; fi
            a=$("$first" run "$scene" "$cpu_ab_rate" "$cpu_ab_seconds" | tr -d '\r')
            b=$("$second" run "$scene" "$cpu_ab_rate" "$cpu_ab_seconds" | tr -d '\r')
            [ "$first" = "$after" ] || { local t=$a; a=$b; b=$t; }
            printf '%s\t%s\t%s\n' "$name" "$a" "$b" >> "$work/loads.tsv"
            scene=$((scene + 1))
        done <<< "$scenes"
        echo "Round $round of $cpu_ab_rounds done"
    done
    echo "Load after: $(load_average)"
    printf '\nAfter/before CPU load ratio per scene, %s rounds of %s s at %s Hz (above 1: slower than %s)\n' \
        "$cpu_ab_rounds" "$cpu_ab_seconds" "$cpu_ab_rate" "$ref"
    awk -F '\t' -v min_change="$cpu_ab_min_change" -v min_agree="$cpu_ab_min_agree" -f scripts/cpu-ab.awk "$work/loads.tsv"
    echo "Each run's loads: $work/loads.tsv (scene, after, before)"
)

cpu_ab_scenes() {
    rm -rf "$1/tests/cpu"
    cp -R tests/cpu "$1/tests/cpu"
    cp tests/engine/Measure.h "$1/tests/engine/Measure.h"
}

# The 1-minute load average and the core count, where the system reports them.
load_average() {
    if [ "$os" = macos ]; then
        echo "$(sysctl -n vm.loadavg | awk '{ print $2 }') on $(sysctl -n hw.ncpu) cores"
    else
        echo "not reported on $os"
    fi
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

# A sanitizer build compiles the dependencies with its own flags, so it builds them in its own folder,
# <dependencies>/<sanitizer>: a dependency build folder that builds with two sets of flags (Catch2's,
# shared by every worktree) leaves the other builds' links failing. The sources already fetched stay
# shared. A sanitizer stage configures with "${sanitizer_deps[@]}" in place of -DFETCHCONTENT_BASE_DIR.
sanitizer_deps() {
    local src name
    sanitizer_deps=(--no-warn-unused-cli "-DFETCHCONTENT_BASE_DIR=$DEPS/$1")
    for src in "$DEPS"/*-src; do
        [ -d "$src" ] || continue
        name=$(basename "$src" -src | tr '[:lower:]' '[:upper:]')
        sanitizer_deps+=("-DFETCHCONTENT_SOURCE_DIR_$name=$src")
    done
}

# The heap one stereo Engine holds after prepare(), at 48 and 192 kHz, from the [footprint] tests
# (tests/engine/FootprintTest.cpp), which fail above their ceilings.
engine_heap() {
    step "Engine heap"
    cmake --build "$BUILD_DIR" --config Release --parallel --target eq1_engine_tests
    local out status=0
    out=$("$(test_exe eq1_engine_tests)" "[footprint]") || status=$?
    if [ "$status" != 0 ]; then
        printf '%s\n' "$out"
        return "$status"
    fi
    grep -E 'Engine heap|Analyzer FIFOs' <<< "$out"
}

# The binary of one format in the shipping build.
shipping_binary() {
    local bundle
    case "$1" in
        VST3) bundle=eq1.vst3 ;;
        AU) bundle=eq1.component ;;
        AAX) bundle=eq1.aaxplugin ;;
        CLAP) bundle=eq1.clap ;;
        Standalone) bundle=eq1.app ;;
    esac
    printf '%s\n' "$BUILD_DIR-size/plugin/eq1_artefacts/Release/$1/$bundle/Contents/MacOS/eq1"
}

# The debug information of each shipping bundle is a .dSYM beside it, which must carry the binary's UUID
# for each architecture and resolve the binary's entry function to a source line (what symbolicating a
# crash log needs): a plugin's factory (the CLAP's is the init function its exported entry table points
# at), the Standalone's main. The stripped binary keeps its exported symbols only; the .dSYM keeps them
# all, at the binary's addresses, so the function's address comes from there.
shipping_symbols() {
    step "Debug symbols: the .dSYM beside each bundle has the binary's UUIDs and resolves its entry function"
    local format arch binary dsym dwarf entry name address resolved
    for format in VST3 AU AAX CLAP Standalone; do
        binary=$(shipping_binary "$format")
        dsym=${binary%/Contents/MacOS/eq1}.dSYM
        dwarf=$dsym/Contents/Resources/DWARF/eq1
        if [ ! -f "$dwarf" ]; then
            echo "$format: no debug symbols at $dsym" >&2
            return 1
        fi
        # Stripping after the link invalidates the linker's ad hoc signature, without which macOS loads no
        # arm64 code; the build signs the stripped binary again.
        codesign --verify "$binary" || { echo "$format: the stripped binary's signature is invalid" >&2; return 1; }
        # The symbol as the symbol table spells it, and as atos prints it (demangled).
        case "$format" in
            VST3) entry=_GetPluginFactory name=GetPluginFactory ;;
            AU) entry=_JuceAUFactory name=JuceAUFactory ;;
            AAX) entry=_ACFRegisterPlugin name=ACFRegisterPlugin ;;
            CLAP) entry=__ZN11ClapAdapterL9clap_initEPKc name='ClapAdapter::clap_init(char const*)' ;;
            Standalone) entry=_main name=main ;;
        esac
        for arch in arm64 x86_64; do
            if [ "$(dwarfdump --uuid --arch="$arch" "$binary" | cut -d' ' -f2)" != "$(dwarfdump --uuid --arch="$arch" "$dwarf" | cut -d' ' -f2)" ]; then
                echo "$format $arch: the .dSYM's UUID is not the binary's" >&2
                return 1
            fi
            address=0x$(nm -arch "$arch" --defined-only "$dwarf" | awk -v s="$entry" '$3 == s { print $1 }')
            resolved=$(atos -arch "$arch" -o "$dwarf" "$address")
            # With the DWARF, atos adds the source line: "GetPluginFactory (in eq1) (juce_VST3_Wrapper.cpp:123)".
            case "$resolved" in
                "$name (in eq1) ("*:[0-9]*")") printf '%-11s %-7s %s\n' "$format" "$arch" "$resolved" ;;
                *) echo "$format $arch: $address resolves to '$resolved', not $name with a source line" >&2; return 1 ;;
            esac
        done
    done
}

# What ships, measured (docs/performance.md, "Footprint"): a report, not a check, except the heap and the
# debug symbols.
# The shipping configuration (Release, EQ1_LTO=ON, Universal) is built in its own folder, BUILD_DIR-size,
# without the tests. Code is attributed by scripts/code-by-library.py from the normal build's objects, so
# BUILD_DIR is built first if it isn't. lipo, strip and nm make the binary reports macOS-only.
size() {
    if [ "$os" != macos ]; then
        step "Footprint"
        echo "Binary size and code by library are measured on macOS only; the Engine heap follows"
        engine_heap
        return
    fi
    [ -f "$BUILD_DIR/CMakeCache.txt" ] || build
    step "Shipping build (Release, EQ1_LTO=ON, Universal) in $BUILD_DIR-size"
    match_generator Ninja
    cmake -S . -B "$BUILD_DIR-size" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
        -DEQ1_LTO=ON -DEQ1_BUILD_TESTS=OFF "-DFETCHCONTENT_BASE_DIR=$DEPS"
    cmake --build "$BUILD_DIR-size" --config Release --parallel

    step "Binary size (bytes): as built, and as strip -x leaves it (the same once the build strips)"
    local scratch=$BUILD_DIR-size/size-report format arch binary thin
    mkdir -p "$scratch"
    printf '%-11s %-10s %12s %12s\n' Format Arch "As built" Stripped
    for format in VST3 AU AAX CLAP Standalone; do
        binary=$(shipping_binary "$format")
        for arch in arm64 x86_64; do
            thin=$scratch/$format-$arch
            lipo -thin "$arch" -output "$thin" "$binary"
            cp "$thin" "$thin-stripped"
            strip -x -no_code_signature_warning "$thin-stripped"
            printf '%-11s %-10s %12s %12s\n' "$format" "$arch" "$(stat -f %z "$thin")" "$(stat -f %z "$thin-stripped")"
        done
        printf '%-11s %-10s %12s\n' "$format" universal "$(stat -f %z "$binary")"
    done

    shipping_symbols

    # The stripped binary keeps its exported symbols only; its .dSYM carries the whole symbol table at the
    # binary's addresses.
    step "Code by library (bytes of the VST3's __text)"
    for arch in arm64 x86_64; do
        echo "-- $arch"
        python3 scripts/code-by-library.py "$(shipping_binary VST3 | sed 's|/Contents/MacOS/eq1$|.dSYM/Contents/Resources/DWARF/eq1|')" "$arch" \
            "$BUILD_DIR/plugin/CMakeFiles/eq1.dir" "$BUILD_DIR/plugin/CMakeFiles/eq1_VST3.dir" \
            "$BUILD_DIR/engine/CMakeFiles/eq1_engine.dir" "$DEPS/clap-juce-extensions-build/libclap_juce_extensions.a"
    done

    engine_heap
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
    sanitizer_deps tsan
    cmake -S . -B "$BUILD_DIR-tsan" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEQ1_BUILD_PLUGIN=OFF \
        "${sanitizer_deps[@]}" -DCMAKE_CXX_FLAGS=-fsanitize=thread -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
    cmake --build "$BUILD_DIR-tsan" --parallel
    TSAN_OPTIONS=halt_on_error=1 ctest --test-dir "$BUILD_DIR-tsan" --output-on-failure -LE 'response|sweep'
}

# Some toolchains' AddressSanitizer runtime never finishes starting (Apple clang 17 on macOS 27 spins in
# its own initialisation), which would hang the build at test discovery: an empty program built with it
# must exit within 30 seconds.
asan_starts() {
    local dir pid
    dir=$(mktemp -d)
    echo 'int main() { return 0; }' > "$dir/empty.cpp"
    c++ -fsanitize=address "$dir/empty.cpp" -o "$dir/empty"
    "$dir/empty" &
    pid=$!
    for _ in $(seq 30); do
        kill -0 "$pid" 2> /dev/null || { rm -rf "$dir"; return 0; }
        sleep 1
    done
    kill -9 "$pid"; wait "$pid" 2> /dev/null || true
    rm -rf "$dir"
    echo "This toolchain's AddressSanitizer runtime hangs starting an empty program ($(c++ --version | head -1))" >&2
    return 1
}

asan() {
    if [ "$os" != macos ]; then
        echo "AddressSanitizer and UndefinedBehaviorSanitizer run on macOS only; skipped"
        return
    fi
    # Catches out-of-bounds and use-after-free accesses, leaks aside (Apple's clang has no LeakSanitizer),
    # and undefined behaviour such as signed overflow, each halting the test with a stack
    # trace. Only the test executables are built, not the plugin formats; the frequency response grids
    # and the sweeps are left to the normal run, as tsan does. Where the toolchain's AddressSanitizer
    # hangs, it says so and runs UndefinedBehaviorSanitizer alone, in its own build and dependency folders,
    # unless EQ1_REQUIRE_ASAN=1 asks for AddressSanitizer: then it fails.
    local sanitizers=address,undefined name=asan
    if ! asan_starts; then
        if [ "${EQ1_REQUIRE_ASAN:-}" = 1 ]; then
            echo "AddressSanitizer is required (EQ1_REQUIRE_ASAN=1) but hangs on this toolchain: failing rather than running UndefinedBehaviorSanitizer alone" >&2
            return 1
        fi
        echo "AddressSanitizer skipped: running the tests under UndefinedBehaviorSanitizer only" >&2
        sanitizers=undefined name=ubsan
    fi
    step "Engine and Plugin Shell tests under sanitizers: $sanitizers"
    local flags="-fsanitize=$sanitizers -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
    sanitizer_deps "$name"
    cmake -S . -B "$BUILD_DIR-$name" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEQ1_LTO=OFF -DEQ1_STRIP=OFF \
        "${sanitizer_deps[@]}" "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_CXX_FLAGS=$flags" \
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=$sanitizers"
    cmake --build "$BUILD_DIR-$name" --parallel --target eq1_engine_tests eq1_plugin_tests
    ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_stack_use_after_return=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        ctest --test-dir "$BUILD_DIR-$name" -j 8 --output-on-failure -LE 'response|sweep'
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
    cpu) if [ -n "${2:-}" ]; then cpu_ab "$2"; else cpu; fi ;;
    paint) paint ;;
    null) null "${2:?usage: scripts/check.sh null <ref>}" ;;
    size) size ;;
    focus) focus "${2:?usage: scripts/check.sh focus <regex>}" ;;
    screens) screens "${2:?usage: scripts/check.sh screens <dir>}" ;;
    tsan) tsan ;;
    asan) asan ;;
    validate) validate ;;
    docs) docs; colours ;;
    colours) colours ;;
    hooks) hooks ;;
    # A busy machine skips the CPU budget and the paint time (exit 3) but not the stages after them.
    all) docs; colours; hooks; build; run_tests; cpu || [ $? -eq 3 ]; paint || [ $? -eq 3 ]; tsan; validate ;;
    *) sed -n '2,45p' "$0" >&2; exit 2 ;;
esac
