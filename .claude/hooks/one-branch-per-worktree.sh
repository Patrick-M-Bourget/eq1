#!/usr/bin/env bash
# Keeps the main checkout on main: another session may be working there, and it shares build/ and
# .deps/. Branch work goes in its own worktree, with its own build/. Switching the main checkout
# back to main, and restoring files (git checkout -- <path>, git checkout ., git restore), are left
# alone. A command that only quotes "git checkout" in a string is blocked too: rare, and the message says why.
set -uo pipefail

input=$(cat)
command=$(jq -r '.tool_input.command // empty' <<< "$input")
dir=$(jq -r '.cwd // empty' <<< "$input")

git_cmd='git([[:space:]]+-C[[:space:]]+[^[:space:];&|]+)?[[:space:]]+'
switch=$(grep -oE "${git_cmd}(switch|checkout)([[:space:]]+[^;&|]*)?" <<< "$command")
grep -qE '(^|[;&|[:space:]])gh[[:space:]]+pr[[:space:]]+checkout' <<< "$command" && blocked=1
[ -n "$switch" ] || [ "${blocked:-0}" = 1 ] || exit 0
while IFS= read -r invocation; do
    args=$(sed -E "s/^${git_cmd}(switch|checkout)//" <<< "$invocation" | xargs)
    [ -n "$invocation" ] || continue
    case " $args " in
        *" -- "* | " . ") continue ;;                  # restores files, keeps the branch
        " main " | " origin/main ") continue ;;        # back to main is fine
    esac
    blocked=1
done <<< "${switch:-}"
[ "${blocked:-0}" = 1 ] || exit 0

# The directory git runs in: the last "cd <dir>" or "git -C <dir>" in the command, else the session's.
last_dir=$(grep -oE '(cd|git[[:space:]]+-C)[[:space:]]+[^[:space:];&|]+' <<< "$command" | tail -1 | awk '{print $NF}')
[ -n "$last_dir" ] && dir=$last_dir
dir=${dir/#\~/$HOME}

git_dir=$(git -C "$dir" rev-parse --absolute-git-dir 2> /dev/null) || exit 0
common_dir=$(cd "$dir" && cd "$(git rev-parse --git-common-dir)" && pwd -P) || exit 0
if [ "$(cd "$git_dir" && pwd -P)" = "$common_dir" ]; then
    echo "BLOCKED: the main checkout stays on main. Work on a branch in its own worktree:" \
        "git worktree add ../eq1-<ticket> -b <branch> origin/main (it gets its own build/)." >&2
    exit 2
fi
exit 0
