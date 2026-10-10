# Reviewing and merging

A PR merges when CI is green on both platforms and a reviewer has passed its current head commit.

## The verdict

The reviewer checks the diff against the ticket's Agent Brief, spec #1 (including its Staple reskin decisions), `CODING_STANDARDS.md`, `GLOSSARY.md` and `docs/adr/`, then posts one PR comment: findings, most severe first, each with `file:line`, a concrete failure and a fix, and on its own line one marker:

```
<!-- qa-verdict: pass sha=<first 7 characters of the head commit> -->
<!-- qa-verdict: changes sha=<first 7 characters of the head commit> -->
```

**changes** is for a brief criterion not met, a bug a user would hit, a test that can't fail, a standards or glossary breach, real-time unsafety in `engine/`, or drift from the plan (including into parked work). Anything smaller rides along in a **pass** as a follow-up. A verdict covers one commit: a rebase or new push needs a new one.

## Merging

`scripts/merge-on-green.sh <PR>` waits for CI, then for a pass verdict on the head commit, and squash-merges as "<title> (#PR)" with "Squashed from #PR". It stops on a conflict with main, failed checks, a changes verdict or a new push. After a merge, other open PRs may conflict: rebase them before they can merge. ThreadSanitizer and macOS plugin validation run on main after each merge, not on PRs: a failure there is fixed forward.
