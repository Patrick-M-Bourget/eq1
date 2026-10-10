# Reviewing and merging

A PR merges when CI is green on both platforms and a reviewer has passed its current head commit.

## The verdict

The reviewer checks the diff against the ticket's Agent Brief, spec #1 (including its Staple reskin decisions), `CODING_STANDARDS.md`, `GLOSSARY.md` and `docs/adr/`, then posts one PR comment: findings, most severe first, each with `file:line`, a concrete failure and a fix, and on its own line one marker:

```
<!-- qa-verdict: pass sha=<first 7 characters of the head commit> -->
<!-- qa-verdict: changes sha=<first 7 characters of the head commit> -->
```

**changes** is for a brief criterion not met, a bug a user would hit, a test that can't fail, a standards or glossary breach, real-time unsafety in `engine/`, or drift from the plan (including into parked work). Anything smaller rides along in a **pass** as a follow-up. A verdict covers one commit: a rebase or new push needs a new one, which reviews the change since the last verdict (and anything that change touches) rather than the whole PR again.

## Merging

A session that built a PR can't review it: when it posts the verdict itself, from a review it ran, the user's go-ahead to merge is the human review. It asks for that go-ahead once CI is green, quotes it in the verdict comment, and runs `merge-on-green.sh` after it.

`scripts/merge-on-green.sh <PR>` waits for CI, then for a pass verdict on the head commit, and squash-merges as "<title> (#PR)" with "Squashed from #PR". It stops on a conflict with main, failed checks, a changes verdict or a new push. After a merge, other open PRs may conflict: rebase them before they can merge. ThreadSanitizer and macOS plugin validation run on main after each merge, not on PRs: a failure there is fixed forward.

## Integration branches

A set of tickets can land on an integration branch (such as `spec1-editor-v1`) with one draft PR into main. Each ticket's branch starts from the integration branch and fast-forwards into it once its own checks pass; a ticket PR into the integration branch, if any, merges the same way. Before the draft leaves draft, merge the PRs into main that are ready, then merge main into the integration branch. The reviewer's verdict on the integration PR goes on that final head; while heads are still moving, hold it.
