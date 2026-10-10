#!/usr/bin/env bash
# Keeps the main checkout on main: another session may be working there, and it shares build/ and
# .deps/. Branch work goes in its own worktree, with its own build/. Switching the main checkout
# back to main (with -q or --detach too), and restoring files (git checkout -- <path>, git checkout .,
# git restore), are left alone. Each part of a compound command (split at ;, &&, || and |) is judged in
# the directory it runs in: its own git -C, else the last cd before it, else the session's. A command
# that only quotes "git checkout" in a string is judged as if it ran it: rare, and the message says why.
# Tests: .claude/hooks/one-branch-per-worktree.test.sh
set -uo pipefail

input=$(cat)
command=$(jq -r '.tool_input.command // empty' <<< "$input")
dir=$(jq -r '.cwd // empty' <<< "$input")

git_cmd='git([[:space:]]+-C[[:space:]]+[^[:space:];&|]+)?[[:space:]]+'
gh_checkout='(^|[[:space:]])gh[[:space:]]+pr[[:space:]]+checkout'
grep -qE "${git_cmd}(switch|checkout)|${gh_checkout}" <<< "$command" || exit 0

# Whether dir is a repository's main checkout, not a linked worktree.
main_checkout() {
    local git_dir common_dir
    git_dir=$(git -C "$1" rev-parse --absolute-git-dir 2> /dev/null) || return 1
    common_dir=$(cd "$1" && cd "$(git rev-parse --git-common-dir)" && pwd -P) || return 1
    [ "$(cd "$git_dir" && pwd -P)" = "$common_dir" ]
}

# Whether a git switch or checkout picks a branch other than main: flags that pick none are left out.
changes_branch() {
    local args
    args=$(sed -E "s/^.*${git_cmd}(switch|checkout)//" <<< "$1" | xargs -n1 2> /dev/null | grep -vxE -- '-q|--quiet|--detach' | xargs)
    case " $args " in
        *" -- "* | " . ") return 1 ;;           # restores files, keeps the branch
        " main " | " origin/main ") return 1 ;; # back to main is fine
    esac
    return 0
}

# dir resolved from base: absolute, ~ or relative.
resolve() {
    local target=${2/#\~/$HOME}
    (cd "$1" 2> /dev/null && cd "$target" 2> /dev/null && pwd) || printf '%s\n' "$target"
}

blocked=0
while IFS= read -r part; do
    part=$(sed -E 's/^[[:space:]]+//' <<< "$part")
    if [[ "$part" =~ ^cd[[:space:]]+([^[:space:]]+) ]]; then
        dir=$(resolve "$dir" "${BASH_REMATCH[1]}")
        continue
    fi
    if grep -qE "${git_cmd}(switch|checkout)" <<< "$part"; then
        changes_branch "$part" || continue
    elif ! grep -qE "$gh_checkout" <<< "$part"; then
        continue
    fi
    here=$dir
    [[ "$part" =~ git[[:space:]]+-C[[:space:]]+([^[:space:]]+) ]] && here=$(resolve "$dir" "${BASH_REMATCH[1]}")
    main_checkout "$here" && blocked=1
done < <(sed -E 's/(&&|\|\||;|\|)/\n/g' <<< "$command")

if [ "$blocked" = 1 ]; then
    echo "BLOCKED: the main checkout stays on main. Work on a branch in its own worktree:" \
        "git worktree add ../eq1-<ticket> -b <branch> origin/main (it gets its own build/)." >&2
    exit 2
fi
exit 0
