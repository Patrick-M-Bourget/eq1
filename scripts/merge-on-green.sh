#!/usr/bin/env bash
# Squash-merges a PR once CI passes and a reviewer has passed its head commit (docs/agents/reviewer.md).
# Stops, saying why, on a conflict with main, failed checks, no checks, a changes verdict or a head that moved
# from the one CI ran on (gh pr merge --match-head-commit also refuses one that moves at the last moment).
#
#   scripts/merge-on-green.sh <PR number>
#   scripts/merge-on-green.sh <PR number> --user-approved "<the user's words>"
#
# --user-approved stands for the verdict when the session that built the PR merges it on the user's
# go-ahead: it waits for CI as usual and records the user's words in the squash commit.
set -uo pipefail
usage='usage: scripts/merge-on-green.sh <PR number> [--user-approved "<the user'"'"'s words>"]'
pr=${1:?$usage}
approval=
if [ $# -gt 1 ]; then
    [ "$2" = --user-approved ] && [ -n "${3:-}" ] || { echo "$usage" >&2; exit 2; }
    approval=$3
fi

sleep 30 # let the checks register
# GitHub runs no CI on a PR that conflicts with main: say so at once rather than wait.
state=UNKNOWN
for _ in 1 2 3; do
    state=$(gh pr view "$pr" --json mergeable -q .mergeable)
    [ "$state" != UNKNOWN ] && break
    sleep 10
done
if [ "$state" = CONFLICTING ]; then
    echo "NOT MERGED #$pr: conflicts with main; rebase it"
    exit 1
fi

# The head CI is about to run on: a push after this is judged by nothing that ran, so it stops the merge.
head=$(gh pr view "$pr" --json headRefOid -q .headRefOid)
head_moved() {
    local now
    now=$(gh pr view "$pr" --json headRefOid -q .headRefOid)
    if [ "$now" != "$head" ]; then
        echo "NOT MERGED #$pr: head moved from ${head:0:7} to ${now:0:7}; run again"
        exit 1
    fi
}

gh pr checks "$pr" --watch --interval 60 > /dev/null 2>&1
# Both platforms' jobs must have run, by exact name: an empty or partial list of checks is not green.
verdict='if length == 0 then "none" else
    [(.[] | select(.bucket != "pass") | "\(.name) (\(.bucket))"),
     ((["Windows x64", "macOS Universal"] - [.[].name])[] | "\(.) (did not run)")]
    | if length == 0 then "green" else "failed: " + join(", ") end end'
checks=$(gh pr checks "$pr" --json name,bucket -q "$verdict" 2>&1)
case "$checks" in
    green) ;;
    none | *"no checks reported"*)
        echo "NOT MERGED #$pr: no checks ran"
        exit 1
        ;;
    "failed: "*)
        echo "NOT MERGED #$pr: checks ${checks}"
        gh pr checks "$pr"
        exit 1
        ;;
    *)
        echo "NOT MERGED #$pr: could not read the checks: $checks"
        exit 1
        ;;
esac
head_moved

title=$(gh pr view "$pr" --json title -q .title)
# GitHub refuses the merge if the head is no longer the one CI ran on.
merge() {
    if gh pr merge "$pr" --squash --match-head-commit "$head" --subject "$title (#$pr)" --body "$1"; then
        echo "MERGED #$pr: $title"
    else
        echo "NOT MERGED #$pr: gh pr merge refused it (has the head moved from ${head:0:7}?)"
        exit 1
    fi
}
if [ -n "$approval" ]; then
    merge "Squashed from #$pr"$'\n\n'"Approved by the user: \"$approval\""
    exit
fi
verdicts() { gh pr view "$pr" --json comments -q '.comments[].body'; }
until verdicts | grep -q "qa-verdict: pass sha=${head:0:7}"; do
    if verdicts | grep -q "qa-verdict: changes sha=${head:0:7}"; then
        echo "NOT MERGED #$pr: the reviewer asked for changes on ${head:0:7}"
        exit 1
    fi
    head_moved
    sleep 60
done

merge "Squashed from #$pr"
