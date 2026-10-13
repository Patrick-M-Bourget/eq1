#!/usr/bin/env bash
# Prints file:line:#N for each place code cites one of the given tickets: a whole #N in a tracked file under
# engine, plugin, tools or tests, Markdown aside, not part of an entity (&#N), a path (a/#N), a word or a longer
# number. scripts/check.sh docs (closed tickets) and scripts/merge-on-green.sh (the tickets a PR closes) share it.
#
#   scripts/cited-tickets.sh [--at <commit>] <ticket number>...
#
# Reads the working tree's tracked files, or with --at a commit's. Run it inside the repo. Exits 0 whether or not
# anything cites them.
set -uo pipefail
at=
if [ "${1:-}" = --at ]; then
    at=${2:?usage: scripts/cited-tickets.sh [--at <commit>] <ticket number>...}
    shift 2
fi
[ $# -gt 0 ] || exit 0
git grep -noP '(?<![A-Za-z0-9&/_])#\d{1,4}(?![0-9A-Za-z])' ${at:+"$at"} -- engine plugin tools tests ':!*.md' |
    { if [ -n "$at" ]; then cut -d: -f2-; else cat; fi; } |
    awk -F: -v list="$*" 'BEGIN { n = split (list, c, " "); for (i = 1; i <= n; i++) cited["#" c[i]] = 1 }
                          ($NF in cited) { print }'
exit 0
