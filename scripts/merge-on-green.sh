#!/usr/bin/env bash
# Squash-merges a PR once CI passes and a reviewer has passed its head commit (docs/agents/reviewer.md).
# Stops, saying why, on a conflict with main, failed checks, no checks, a changes verdict or a head that moved
# from the one CI ran on (gh pr merge --match-head-commit also refuses one that moves at the last moment).
# It also refuses a head whose code still cites a ticket the PR closes (its closingIssuesReferences, or a
# Closes/Fixes/Resolves #N in its body), listing each "file:line: points at #N, which this PR closes": merged,
# that citation would point at a closed ticket, which scripts/check.sh docs refuses on main. The rule for a
# citation is scripts/cited-tickets.sh's. --user-approved doesn't override it: it's a content error, not a verdict.
#
#   scripts/merge-on-green.sh <PR number>
#   scripts/merge-on-green.sh <PR number> --user-approved "<the user's words>"
#
# --user-approved stands for the verdict when the session that built the PR merges it on the user's
# go-ahead: it waits for CI as usual and records the user's words in the squash commit.
# After a merge it deletes the PR's branch, on origin and locally.
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

# Code citing a ticket this PR closes would cite a closed ticket once merged (scripts/check.sh docs).
if ! closes=$(gh pr view "$pr" --json closingIssuesReferences,body -q '[.closingIssuesReferences[].number,
    (.body | scan("(?:^|[^A-Za-z])(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?):?\\s+#([0-9]+)"; "i") | .[0] | tonumber)]
    | unique | .[]'); then
    echo "NOT MERGED #$pr: could not read which tickets it closes"
    exit 1
fi
if [ -n "$closes" ]; then
    if ! git fetch -q origin "refs/pull/$pr/head"; then
        echo "NOT MERGED #$pr: could not fetch its head to read its files"
        exit 1
    fi
    # shellcheck disable=SC2086 # one ticket number per word
    if ! cited=$("$(dirname "$0")/cited-tickets.sh" --at "$head" $closes); then
        echo "NOT MERGED #$pr: could not read its head's files (has it moved from ${head:0:7}?)"
        exit 1
    fi
    if [ -n "$cited" ]; then
        sed -E 's/:(#[0-9]+)$/: points at \1, which this PR closes/' <<< "$cited"
        echo "NOT MERGED #$pr: its head cites tickets it closes; say what the code does now, or point at an open ticket"
        exit 1
    fi
fi

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
branch=$(gh pr view "$pr" --json headRefName -q .headRefName)
# A squash merge leaves the PR's branch behind, on origin and here: delete both. A branch still checked out in
# a worktree stays, and says so.
delete_branch() {
    gh api -X DELETE "repos/{owner}/{repo}/git/refs/heads/$branch" > /dev/null 2>&1 \
        || echo "kept origin/$branch: could not delete it"
    git show-ref -q --verify "refs/heads/$branch" || return 0
    if git worktree list --porcelain | grep -qx "branch refs/heads/$branch"; then
        echo "kept $branch: checked out in a worktree"
    else
        git branch -q -D "$branch"
    fi
}
# GitHub refuses the merge if the head is no longer the one CI ran on.
merge() {
    if gh pr merge "$pr" --squash --match-head-commit "$head" --subject "$title (#$pr)" --body "$1"; then
        echo "MERGED #$pr: $title"
        delete_branch
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
