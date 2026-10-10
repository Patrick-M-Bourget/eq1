#!/usr/bin/env bash
# Shows images on a PR, which gh can't upload: commits them to the orphan branch pr-assets, under
# pr-<number>/, without checking it out, pushes it, and posts a comment embedding them.
#
#   scripts/pr-images.sh <PR number> <image>...
set -euo pipefail
usage='usage: scripts/pr-images.sh <PR number> <image>...'
pr=${1:?$usage}
shift
[ $# -gt 0 ] || { echo "$usage" >&2; exit 2; }
repo=$(gh repo view --json nameWithOwner -q .nameWithOwner)
branch=pr-assets

parent=
if git fetch -q origin "$branch" 2> /dev/null; then
    parent=$(git rev-parse FETCH_HEAD)
fi
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
export GIT_INDEX_FILE=$scratch/index
[ -n "$parent" ] && git read-tree "$parent"
body="Images:"
for image in "$@"; do
    name=$(basename "$image")
    git update-index --add --cacheinfo "100644,$(git hash-object -w "$image"),pr-$pr/$name"
    body+=$'\n\n'"**$name**"$'\n'"![$name](https://raw.githubusercontent.com/$repo/$branch/pr-$pr/$name)"
done
commit=$(git commit-tree "$(git write-tree)" ${parent:+-p "$parent"} -m "Images for #$pr")
unset GIT_INDEX_FILE
git push -q origin "$commit:refs/heads/$branch"
gh pr comment "$pr" --body "$body"
