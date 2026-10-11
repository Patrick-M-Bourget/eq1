#!/usr/bin/env bash
# Tests .githooks/pre-push with real pushes between throwaway repositories, its gate (.githooks/gate.sh)
# replaced by a stub that counts its runs.
# Run: scripts/check.sh hooks (by hand, or in a full check; never from a git hook).
set -uo pipefail
# Inside a git hook, GIT_DIR and GIT_INDEX_FILE point at the repository being committed to; left set,
# the throwaway repositories' git commands would land there.
unset $(git rev-parse --local-env-vars)
hooks=$(cd "$(dirname "$0")/../.githooks" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1
export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
export GATE_RUNS=$tmp/gate-runs

git init -q --bare -b main "$tmp/remote.git"
git init -q -b main "$tmp/seed"
mkdir -p "$tmp/seed/.githooks" "$tmp/seed/engine"
cp "$hooks/pre-push" "$hooks/build-inputs.sh" "$tmp/seed/.githooks/"
printf '#!/usr/bin/env bash\necho run >> "$GATE_RUNS"\n' > "$tmp/seed/.githooks/gate.sh"
chmod +x "$tmp/seed/.githooks/gate.sh"
echo 1 > "$tmp/seed/engine/a.cpp"
echo readme > "$tmp/seed/README.md"
git -C "$tmp/seed" add -A
git -C "$tmp/seed" commit -q -m init
git -C "$tmp/seed" push -q "$tmp/remote.git" main
git clone -q "$tmp/remote.git" "$tmp/here"
git clone -q "$tmp/remote.git" "$tmp/elsewhere"
here=$tmp/here
if [ "$(git -C "$here" rev-parse --show-toplevel 2> /dev/null)" != "$(cd "$here" && pwd -P)" ]; then
    echo "pre-push.test.sh: the throwaway repository isn't separate; stopping" >&2
    exit 1
fi
git -C "$here" config core.hooksPath .githooks

# commit <repo> <file> <content>
commit() {
    echo "$3" > "$1/$2"
    git -C "$1" add "$2"
    git -C "$1" commit -q -m "$2"
}

failures=0
# expect <pass|refuse> <gate runs> <what> <git args...>: runs git in the repository here.
expect() {
    local want=$1 runs=$2 what=$3 got=pass err
    shift 3
    rm -f "$GATE_RUNS"
    err=$(git -C "$here" "$@" 2>&1 > /dev/null) || got=refuse
    local ran=0
    [ -f "$GATE_RUNS" ] && ran=$(wc -l < "$GATE_RUNS" | tr -d ' ')
    if [ "$got" != "$want" ] || [ "$ran" != "$runs" ]; then
        echo "FAIL: $what: expected $want with $runs gate runs, got $got with $ran" >&2
        printf '%s\n' "$err" | sed 's/^/    /' >&2
        failures=$((failures + 1))
    elif printf '%s\n' "$err" | grep -q '^fatal:'; then
        echo "FAIL: $what: git printed a fatal error" >&2
        printf '%s\n' "$err" | sed 's/^/    /' >&2
        failures=$((failures + 1))
    fi
}

# A remote tip pushed from elsewhere, which this repository never fetched, then overwritten.
commit "$tmp/elsewhere" engine/a.cpp elsewhere
git -C "$tmp/elsewhere" push -q origin main
commit "$here" engine/a.cpp here
expect pass 1 "unknown remote tip, forced" push -q --force origin main
git -C "$here" fetch -q

# A tag on a commit with code changes the remote hasn't seen, and that isn't checked out.
commit "$here" engine/a.cpp tagged
git -C "$here" tag v1
git -C "$here" reset -q --hard HEAD~1
expect pass 0 "every tag" push -q --tags origin

git -C "$here" switch -q -c docs origin/main
commit "$here" README.md docs
git -C "$here" switch -q -c feature origin/main
commit "$here" engine/a.cpp feature
expect pass 1 "the checked-out branch and a docs-only one" push -q origin feature docs
expect pass 0 "a docs-only branch, not checked out" push -q origin docs:docs2

git -C "$here" switch -q -c other origin/main
commit "$here" engine/b.cpp other
git -C "$here" switch -q feature
expect refuse 0 "a code branch, not checked out" push -q origin other
expect refuse 0 "the checked-out branch and a code branch, not checked out" push -q origin feature:feature2 other

if [ "$failures" = 0 ]; then
    echo "The pre-push hook gates the checked-out branch once, skips tags and refuses untested code branches"
else
    exit 1
fi
