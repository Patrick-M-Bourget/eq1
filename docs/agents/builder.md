# Building a ticket

How an agent takes one ready-for-agent ticket to an open PR. The ticket's latest `## Agent Brief` comment is its spec.

## Steps

1. **Worktree.** A branch in a linked worktree (`CLAUDE.md`, Worktrees), or a fresh branch from `origin/main` in an idle one, whose build is already warm. A new worktree needs `scripts/check.sh build` once; it shares the main checkout's dependencies on its own. Done when the branch is checked out and built.
2. **Read.** The brief, `GLOSSARY.md` (its terms in names, comments, tests and UI text), `CODING_STANDARDS.md`, `docs/adr/`, the code the brief names, and what an editor change builds on: the Staple kit in `plugin/staple/` and the test harness in `tests/plugin/EditorHarness.h`. Done when you can name the seam each acceptance criterion is tested at.
3. **Red, then green.** One vertical slice at a time at the brief's seams (`/tdd`): write the test, watch it fail, write the code. Run `scripts/check.sh focus '<regex>'` while working. Validation, ThreadSanitizer and the response grids are CI's; so is the CPU budget whenever other builds share the machine. Done when every acceptance criterion has a test you saw go red, or a reason it can't (drawing, host-only behaviour) recorded for the PR.
4. **Commit.** Small commits, each one change; the pre-commit hook builds and runs the tests and must pass as is. Done when the working tree is clean.
5. **Rebase.** `git fetch -q && git rebase origin/main`, keeping both sides where parallel tickets added beside each other. The pre-push hook gates the result. Done when the push succeeds.
6. **PR.** Open it with `/pr`. Write "Closes #N", list what only a host shows (for the user's smoke test in Live), and for a Staple reskin slice attach a screenshot against the prototype. A ticket left unfinished goes up as a draft with "Part of #N" and its open criteria listed. Done when the PR is open; merging is the coordinator's (`docs/agents/reviewer.md`).

## Running several builders

Each builder gets its own worktree and touches different files where the plan allows. On one 8-core Mac, run at most three: past that, builds and tests slow every builder down several times over. The worktrees share the main checkout's `.deps`, dependencies' build folders included (Catch2, the CLAP extensions): two builds that reach them at once can truncate Ninja's log there (`ninja: warning: premature end of file`), and the next build recompiles those dependencies. That costs time, not correctness.

## A Staple reskin slice

The handoff (`docs/staple-handoff/`) decides how it looks; spec #1 decides what it does, and its "Staple reskin" list names where they differ.

- **Render the prototype** to compare against images, not markup: `python3 -m http.server -d docs/staple-handoff/prototype 8765`, then open `http://localhost:8765/Main.dc.html` (or `DesignSystem.dc.html` for single controls). For a PNG at the editor's size: Chrome `--headless=new --window-size=1200,760 --force-device-scale-factor=2 --screenshot=<file.png> <url>`.
- **Render the editor** with the hidden `[.screens]` tests (`scripts/check.sh screens <dir>`) at the same size and scale, and check them side by side area by area. The PR attaches both.
- Every colour and size comes from `plugin/staple/Tokens.h`; the editor has one LookAndFeel, `staple::LookAndFeel`.
