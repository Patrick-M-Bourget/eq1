#!/usr/bin/env bash
# Tests merge-on-green.sh against a fake gh (and a sleep that returns at once) on PATH: no PR is read or merged.
# Run: scripts/check.sh hooks (by hand, or in a full check; never from a git hook). Needs jq.
#
# The fake reads its PR from files in $FAKE: mergeable, title, comments (JSON array of bodies), checks
# (JSON array of {name, bucket}; [] is answered as gh does, "no checks reported") and heads (one SHA per line:
# each read of headRefOid takes the next, the last repeating, so a later line is a push landing meanwhile).
# The tickets the PR closes come from closes (JSON array of numbers, its closingIssuesReferences) and body; its
# head's files are a scratch repo's, whose commits are $a (citing no ticket) and $c (citing #12 in engine/a.cpp),
# read through the fake git's grep (fetches are logged to fetch.log, and fail if $FAKE/no-fetch).
# gh pr merge is logged to merge.log, and refused when --match-head-commit isn't the PR's head. Branch deletions,
# through gh api and a fake git (local branch "topic" exists unless $FAKE/no-local, and is checked out in a
# worktree if $FAKE/checked-out), are logged to deleted.log.
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
    "pr view headRefName") echo topic ;;
    "pr view closingIssuesReferences,body")
        jq -n --argjson c "$(cat "$FAKE/closes")" --arg b "$(cat "$FAKE/body")" \
            '{closingIssuesReferences: $c | map({number: .}), body: $b}' | jq -r "$query"
        ;;
    "api -X ") echo "origin ${args[3]##*/}" >> "$FAKE/deleted.log" ;;
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
cat > "$tmp/bin/git" << 'EOF'
#!/usr/bin/env bash
case "$1 $2" in
    "fetch "* | "fetch -q") echo "$*" >> "$FAKE/fetch.log"; [ ! -e "$FAKE/no-fetch" ]; exit ;;
    "grep "*) exec "$REAL_GIT" -C "$REPO" "$@" ;;
    "show-ref -q") [ ! -e "$FAKE/no-local" ]; exit ;;
    "worktree list") [ -e "$FAKE/checked-out" ] && printf 'worktree /x\nbranch refs/heads/topic\n' ;;
    "branch -q") echo "local $4" >> "$FAKE/deleted.log" ;;
    *) echo "fake git: unexpected call: $*" >&2; exit 99 ;;
esac
exit 0
EOF
chmod +x "$tmp/bin/sleep" "$tmp/bin/gh" "$tmp/bin/git"

export REAL_GIT REPO=$tmp/repo
REAL_GIT=$(command -v git)
mkdir -p "$REPO/engine"
git -C "$REPO" init -q
printf '// Holds.\n' > "$REPO/engine/a.cpp"
git -C "$REPO" add . && git -C "$REPO" -c user.name=t -c user.email=t@t commit -qm clean
a=$(git -C "$REPO" rev-parse HEAD)
printf '// Holds until #12 lands.\n' > "$REPO/engine/a.cpp"
git -C "$REPO" -c user.name=t -c user.email=t@t commit -qam cites
c=$(git -C "$REPO" rev-parse HEAD)
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
    echo "[]" > "$FAKE/closes"
    echo "Does a thing." > "$FAKE/body"
}
# expect <exit code> <output pattern> <merged|unmerged> <description> [script arguments...]
expect() {
    local code=$1 pattern=$2 merged=$3 what=$4 got
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

# deleted <expected deleted.log> <description>
deleted() {
    if [ "$(cat "$FAKE/deleted.log" 2> /dev/null)" != "$1" ]; then
        echo "FAIL: $2: deleted $(cat "$FAKE/deleted.log" 2> /dev/null || echo nothing)" >&2
        failures=$((failures + 1))
    fi
}
deleted $'origin topic\nlocal topic' "a merge deletes the branch on origin and locally"

pr "$green" "$a"
touch "$FAKE/checked-out"
expect 0 "kept topic: checked out in a worktree" merged "a branch checked out in a worktree stays"
deleted "origin topic" "a branch checked out in a worktree stays"

pr "$green" "$a"
touch "$FAKE/no-local"
expect 0 "^MERGED #7" merged "a branch with no local copy merges"
deleted "origin topic" "a branch with no local copy is deleted on origin only"

pr "$green" "$a"$'\n'"$b"
expect 1 "head moved from ${a:0:7} to ${b:0:7}" unmerged "a push while CI ran stops the merge"
deleted "" "an unmerged PR keeps its branch"

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

pr "$green" "$c" "[\"qa-verdict: pass sha=${c:0:7}\"]"
echo "[12]" > "$FAKE/closes"
expect 1 "^engine/a.cpp:1: points at #12, which this PR closes$" unmerged "a PR whose head cites a ticket it closes isn't merged"
grep -q "NOT MERGED #7: its head cites tickets it closes" <<< "$out" || {
    echo "FAIL: the refusal doesn't say why: $out" >&2
    failures=$((failures + 1))
}

pr "$green" "$c" "[]"
printf 'Does a thing.\n\nFixes #12\n' > "$FAKE/body"
expect 1 "^engine/a.cpp:1: points at #12, which this PR closes$" unmerged \
    "the user's go-ahead doesn't merge a head that cites a ticket the body's Fixes line closes" --user-approved "merge it"

pr "$green" "$a"
echo "[12]" > "$FAKE/closes"
printf 'closes #13\n' > "$FAKE/body"
expect 0 "^MERGED #7" merged "a PR closing tickets its head doesn't cite merges"
grep -q "refs/pull/7/head" "$FAKE/fetch.log" 2> /dev/null || {
    echo "FAIL: the PR's head wasn't fetched to read its files" >&2
    failures=$((failures + 1))
}

pr "$green" "$c" "[\"qa-verdict: pass sha=${c:0:7}\"]"
echo "[12]" > "$FAKE/closes"
touch "$FAKE/no-fetch"
expect 1 "could not fetch its head" unmerged "a head it can't fetch isn't merged"

pr "$green" "$b" "[\"qa-verdict: pass sha=${b:0:7}\"]"
echo "[12]" > "$FAKE/closes"
expect 1 "could not read" unmerged "a head whose files it can't read isn't merged"

if [ "$failures" = 0 ]; then
    echo "merge-on-green.sh merges only the head CI passed, with every required job by name"
else
    exit 1
fi
