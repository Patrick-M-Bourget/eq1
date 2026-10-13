#!/usr/bin/env bash
# Tests scripts/cited-tickets.sh, the rule scripts/check.sh docs and merge-on-green.sh share for which code
# cites a ticket, in a scratch repo.
# Run: scripts/check.sh hooks (by hand, or in a full check; never from a git hook).
set -uo pipefail
script=$(cd "$(dirname "$0")" && pwd)/cited-tickets.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
failures=0
cd "$tmp" || exit 1
git init -q .
git config user.email test@example.com
git config user.name test
mkdir engine plugin tools tests docs
cat > engine/a.cpp << 'X'
// Until #12 lands, this holds.
// See #123 and #9.
X
cat > tests/b.cpp << 'X'
auto c = "&#12; a/#12 x_#12 #12a #12b";
// (#12)
X
printf '# Notes\n#12\n' > engine/notes.md
printf '// #12\n' > docs/c.cpp
git add . && git commit -qm one
first=$(git rev-parse HEAD)
printf '// #9 again\n' > plugin/d.cpp
git add . && git commit -qm two
printf '// #12 uncommitted\n' >> plugin/d.cpp

# expect <expected output> <description> <arguments...>
expect() {
    local want=$1 what=$2 got
    shift 2
    got=$("$script" "$@" 2>&1)
    local code=$?
    if [ "$code" != 0 ] || [ "$got" != "$want" ]; then
        echo "FAIL: $what: expected exit 0 and:" >&2
        sed 's/^/    /' <<< "$want" >&2
        echo "  got exit $code and:" >&2
        sed 's/^/    /' <<< "$got" >&2
        failures=$((failures + 1))
    fi
}

expect $'engine/a.cpp:1:#12\nplugin/d.cpp:2:#12\ntests/b.cpp:2:#12' \
    "a whole ticket number in code cites it; an entity, a path, a longer number, a word or Markdown doesn't" 12
expect $'engine/a.cpp:2:#123\nengine/a.cpp:2:#9\nplugin/d.cpp:1:#9' "several tickets at once" 9 123
expect "" "no citation, no output and no failure" 77
expect "engine/a.cpp:2:#9" "--at reads a commit's files, not the working tree's" --at "$first" 9

if "$script" --at 0123456789abcdef0123456789abcdef01234567 9 > /dev/null 2>&1; then
    echo "FAIL: a commit git can't read should fail, not pass as citing nothing" >&2
    failures=$((failures + 1))
fi

if [ "$failures" = 0 ]; then
    echo "cited-tickets.sh finds the code that cites a ticket, in the working tree or at a commit"
else
    exit 1
fi
