#!/usr/bin/env bash
# Squash-merges a PR once CI passes and a reviewer has passed its head commit (docs/agents/reviewer.md).
# Stops, saying why, on a conflict with main, failed checks, a changes verdict or a new push.
#
#   scripts/merge-on-green.sh <PR number>
set -uo pipefail
pr=${1:?usage: scripts/merge-on-green.sh <PR number>}

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

gh pr checks "$pr" --watch --interval 60 > /dev/null 2>&1
if ! gh pr checks "$pr" --json bucket -q 'all(.[]; .bucket == "pass")' | grep -qx true; then
    echo "NOT MERGED #$pr: checks failed"
    gh pr checks "$pr"
    exit 1
fi

head=$(gh pr view "$pr" --json headRefOid -q .headRefOid)
verdicts() { gh pr view "$pr" --json comments -q '.comments[].body'; }
until verdicts | grep -q "qa-verdict: pass sha=${head:0:7}"; do
    if verdicts | grep -q "qa-verdict: changes sha=${head:0:7}"; then
        echo "NOT MERGED #$pr: the reviewer asked for changes on ${head:0:7}"
        exit 1
    fi
    if [ "$(gh pr view "$pr" --json headRefOid -q .headRefOid)" != "$head" ]; then
        echo "NOT MERGED #$pr: new commits were pushed; run again"
        exit 1
    fi
    sleep 60
done

title=$(gh pr view "$pr" --json title -q .title)
gh pr merge "$pr" --squash --subject "$title (#$pr)" --body "Squashed from #$pr" && echo "MERGED #$pr: $title"
