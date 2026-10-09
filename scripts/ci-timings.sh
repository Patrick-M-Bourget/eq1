#!/usr/bin/env bash
# How long each CI job and step took, for timing CI changes before and after:
#
#   scripts/ci-timings.sh <run-id> [attempt]     attempt defaults to the latest
#
# A job that ran no steps prints its annotations instead: why it never started (billing, a runner).
set -euo pipefail
run=${1:?usage: scripts/ci-timings.sh <run-id> [attempt]}
repo=$(gh repo view --json nameWithOwner --jq .nameWithOwner)
attempt=${2:-$(gh api "repos/$repo/actions/runs/$run" --jq .run_attempt)}

gh api "repos/$repo/actions/runs/$run/attempts/$attempt/jobs" --jq '
  def minutes(a; b): if a and b then ((b | fromdateiso8601) - (a | fromdateiso8601)) as $s | "\($s / 60 | floor)m\($s % 60 | floor)s" else "-" end;
  .jobs[] | "J \(.id) \(.steps | length) \(.name): \(.conclusion // .status), \(minutes(.started_at; .completed_at))",
    (.steps[] | "S   \(.name): \(minutes(.started_at; .completed_at))")' |
while read -r kind rest; do
    if [ "$kind" = J ]; then
        read -r id steps line <<< "$rest"
        printf '%s\n' "$line"
        if [ "$steps" = 0 ]; then
            gh api "repos/$repo/check-runs/$id/annotations" --jq '.[] | "  ! \(.message)"'
        fi
    else
        printf '  %s\n' "$rest"
    fi
done
