#!/usr/bin/env bash
# Tests merge-on-green.sh against a fake gh (and a sleep that returns at once) on PATH: no PR is read or merged.
# Run: scripts/check.sh hooks (by hand, or in a full check; never from a git hook). Needs jq.
#
# The fake reads its PR from files in $FAKE: mergeable, title, comments (JSON array of bodies), checks
# (JSON array of {name, bucket}; [] is answered as gh does, "no checks reported") and heads (one SHA per line:
# each read of headRefOid takes the next, the last repeating, so a later line is a push landing meanwhile).
# gh pr merge is logged to merge.log, and refused when --match-head-commit isn't the PR's head.
set -uo pipefail
script=$(cd "$(dirname "$0")" && pwd)/merge-on-green.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/bin"

cat > "$tmp/bin/sleep" << 'EOF'
#!/usr/bin/env bash
EOF
cat > "$tmp/bin/gh" << 'EOF'
#!/usr/bin/env bash
set -uo pipefail
query=. fields=
args=("$@")
for ((i = 0; i < ${#args[@]}; i++)); do
    case ${args[i]} in
        -q) query=${args[i + 1]} ;;
        --json) fields=${args[i + 1]} ;;
    esac
done
current_head() {
    local n
    n=$(cat "$FAKE/head_reads" 2> /dev/null || echo 0)
    sed -n "$((n + 1))p" "$FAKE/heads" | grep . || tail -n 1 "$FAKE/heads"
}
case "$1 $2 $fields" in
    "pr view mergeable") jq -n --arg m "$(cat "$FAKE/mergeable")" '{mergeable: $m}' | jq -r "$query" ;;
    "pr view title") jq -n --arg t "$(cat "$FAKE/title")" '{title: $t}' | jq -r "$query" ;;
    "pr view comments") jq '{comments: map({body: .})}' "$FAKE/comments" | jq -r "$query" ;;
    "pr view headRefOid")
        jq -n --arg h "$(current_head)" '{headRefOid: $h}' | jq -r "$query"
        echo $(($(cat "$FAKE/head_reads" 2> /dev/null || echo 0) + 1)) > "$FAKE/head_reads"
        ;;
    "pr checks name,bucket")
        if [ "$(jq length "$FAKE/checks")" = 0 ]; then
            echo "no checks reported on the 'topic' branch" >&2
            exit 1
        fi
        jq -r "$query" "$FAKE/checks"
        ;;
    "pr checks ") jq -r '.[] | "\(.name)\t\(.bucket)"' "$FAKE/checks" ;;
    "pr merge ")
        printf '%s\n' "$*" >> "$FAKE/merge.log"
        for ((i = 0; i < ${#args[@]}; i++)); do
            [ "${args[i]}" = --match-head-commit ] && expected=${args[i + 1]}
        done
        [ "${expected:-}" = "$(tail -n 1 "$FAKE/heads")" ] || { echo "head mismatch" >&2; exit 1; }
        ;;
    *) echo "fake gh: unexpected call: $*" >&2; exit 99 ;;
esac
EOF
chmod +x "$tmp/bin/sleep" "$tmp/bin/gh"

a=aaaaaaa1111111111111111111111111111111111
b=bbbbbbb2222222222222222222222222222222222
green='[{"name":"Windows x64","bucket":"pass"},{"name":"macOS Universal","bucket":"pass"}]'
failures=0
run=0
# pr <checks JSON> <heads, one per line> [comments JSON]: sets up a fresh fake PR
pr() {
    run=$((run + 1))
    export FAKE=$tmp/pr$run
    mkdir "$FAKE"
    echo MERGEABLE > "$FAKE/mergeable"
    echo "Do a thing" > "$FAKE/title"
    printf '%s\n' "$1" > "$FAKE/checks"
    printf '%s\n' "$2" > "$FAKE/heads"
    printf '%s\n' "${3:-[\"qa-verdict: pass sha=${a:0:7}\"]}" > "$FAKE/comments"
}
# expect <exit code> <output pattern> <merged|unmerged> <description> [script arguments...]
expect() {
    local code=$1 pattern=$2 merged=$3 what=$4 out got
    shift 4
    out=$(PATH="$tmp/bin:$PATH" "$script" 7 "$@" 2>&1)
    got=$?
    if [ "$got" != "$code" ] || ! grep -qE -- "$pattern" <<< "$out"; then
        echo "FAIL: $what: expected exit $code and /$pattern/, got exit $got:" >&2
        sed 's/^/    /' <<< "$out" >&2
        failures=$((failures + 1))
    fi
    if [ "$merged" = merged ] && ! grep -q -- '--match-head-commit' "$FAKE/merge.log" 2> /dev/null; then
        echo "FAIL: $what: gh pr merge wasn't called" >&2
        failures=$((failures + 1))
    elif [ "$merged" = unmerged ] && [ -s "$FAKE/merge.log" ]; then
        echo "FAIL: $what: gh pr merge was called" >&2
        failures=$((failures + 1))
    fi
}

pr "$green" "$a"
expect 0 "^MERGED #7: Do a thing$" merged "a green PR with a pass verdict merges"
if ! grep -qx -- "pr merge 7 --squash --match-head-commit $a --subject Do a thing (#7) --body Squashed from #7" "$FAKE/merge.log"; then
    echo "FAIL: the squash message changed: $(cat "$FAKE/merge.log")" >&2
    failures=$((failures + 1))
fi

pr "$green" "$a" "[]"
expect 0 "^MERGED #7" merged "the user's go-ahead stands for the verdict" --user-approved "merge it"
grep -q 'Approved by the user: "merge it"' "$FAKE/merge.log" || {
    echo "FAIL: the user's words aren't in the squash commit" >&2
    failures=$((failures + 1))
}

pr "$green" "$a"$'\n'"$b"
expect 1 "head moved from ${a:0:7} to ${b:0:7}" unmerged "a push while CI ran stops the merge"

pr "$green" "$a"$'\n'"$a"$'\n'"$a"$'\n'"$b" "[]"
expect 1 "head moved from ${a:0:7} to ${b:0:7}" unmerged "a push while waiting for the verdict stops the merge"

pr "$green" "$a"$'\n'"$a"$'\n'"$a"$'\n'"$b"
expect 1 "refused" merged "gh pr merge refuses a head that moved at the last moment" --user-approved "merge it"

pr '[{"name":"Windows x64","bucket":"pass"},{"name":"macOS Universal-extra","bucket":"pass"}]' "$a"
expect 1 "checks failed: macOS Universal \(did not run\)" unmerged "a job whose name contains a required one's isn't it"

pr '[{"name":"Windows x64","bucket":"fail"},{"name":"macOS Universal","bucket":"pass"}]' "$a"
expect 1 "checks failed: Windows x64 \(fail\)" unmerged "a failed job stops the merge"

pr '[]' "$a"
expect 1 "no checks ran" unmerged "no checks at all isn't green"

pr "$green" "$a" "[\"qa-verdict: changes sha=${a:0:7}\"]"
expect 1 "asked for changes on ${a:0:7}" unmerged "a changes verdict stops the merge"

pr "$green" "$a"
echo CONFLICTING > "$FAKE/mergeable"
expect 1 "conflicts with main" unmerged "a conflict with main stops the merge"

if [ "$failures" = 0 ]; then
    echo "merge-on-green.sh merges only the head CI passed, with every required job by name"
else
    exit 1
fi
