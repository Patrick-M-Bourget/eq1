#!/usr/bin/env bash
# Tests one-branch-per-worktree.sh against a throwaway repository with one linked worktree.
# Run: .claude/hooks/one-branch-per-worktree.test.sh (scripts/check.sh docs runs it too).
set -uo pipefail
hook=$(cd "$(dirname "$0")" && pwd)/one-branch-per-worktree.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
main=$tmp/repo
linked=$tmp/linked
git init -q -b main "$main"
git -C "$main" -c user.name=t -c user.email=t@t commit -q --allow-empty -m init
git -C "$main" worktree add -q --detach "$linked"

failures=0
# expect <allow|block> <session dir> <command>
expect() {
    local want=$1 cwd=$2 command=$3 got=allow
    jq -n --arg c "$command" --arg d "$cwd" '{tool_input: {command: $c}, cwd: $d}' | "$hook" 2> /dev/null || got=block
    if [ "$got" != "$want" ]; then
        echo "FAIL: expected $want, got $got: (in ${cwd#"$tmp"/}) $command" >&2
        failures=$((failures + 1))
    fi
}

expect block "$main" "git checkout -b topic origin/main"
expect block "$main" "git switch topic"
expect block "$main" "gh pr checkout 12"
expect block "$linked" "cd $main && git checkout topic"
expect block "$linked" "git -C $main switch topic"
expect block "$main" "cd $linked && git status; cd $main && git checkout topic"
expect allow "$main" "git checkout main"
expect allow "$main" "git switch -q --detach origin/main"
expect allow "$main" "git checkout -- plugin/EqDisplay.cpp"
expect allow "$main" "git checkout ."
expect allow "$main" "git status"
expect allow "$linked" "git checkout -b topic origin/main"
expect allow "$main" "cd $linked && git checkout -q -b topic origin/main"
expect allow "$main" "cd $linked && git checkout -q --detach origin/main; cd $main && git pull -q --ff-only"
expect allow "$main" "git -C $linked switch -q --detach origin/main && git -C $main branch -D topic"
expect allow "$main" "cd ../linked && git switch topic"

if [ "$failures" = 0 ]; then
    echo "The worktree hook blocks branch changes in the main checkout only"
else
    exit 1
fi
