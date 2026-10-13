# Building a ticket

How an agent takes one ready-for-agent ticket to an open PR. The ticket's latest `## Agent Brief` comment is its spec.

## Steps

1. **Worktree.** A branch in a linked worktree (`CLAUDE.md`, Worktrees), or a fresh branch from `origin/main` in an idle one, whose build is already warm. A new worktree needs `scripts/check.sh build` once; it shares the main checkout's dependencies on its own. Done when the branch is checked out and built.
2. **Read.** The brief, `GLOSSARY.md` (its terms in names, comments, tests and UI text), `CODING_STANDARDS.md`, `docs/adr/`, the code the brief names, and what an editor change builds on: the Staple kit in `plugin/staple/` and the test harness in `tests/plugin/EditorHarness.h`. Done when you can name the seam each acceptance criterion is tested at.
3. **Red, then green.** One vertical slice at a time at the brief's seams (`/tdd`): write the test, watch it fail, write the code. Run `scripts/check.sh focus '<regex>'` while working. Validation, ThreadSanitizer and the response grids are CI's; so is the CPU budget's absolute ceiling whenever other builds share the machine. A brief's "CPU scenes before/after" is `scripts/check.sh cpu <ref>` (`docs/performance.md`, "CPU A/B"), which runs on a busy machine too. An Engine change on a path no render scene reaches (`tests/render/RenderScenes.h`) adds the scene first: the null check is exact on scenes that never run the change, which proves nothing (#174's Band ↔ Free switch was exact on every scene and −4 dBFS on its own). Done when every acceptance criterion has a test you saw go red, or a reason it can't (drawing, host-only behaviour) recorded for the PR.
4. **Commit.** Small commits, each one change; the pre-commit hook builds and runs the tests and must pass as is. Done when the working tree is clean.
5. **Rebase.** `git fetch -q && git rebase origin/main`, keeping both sides where parallel tickets added beside each other. The pre-push hook gates the result. Done when the push succeeds.
6. **PR.** Open it with `/pr`. Write "Closes #N", list what only a host shows (for the user's smoke test in Live), and for a Staple reskin slice attach a screenshot against the prototype with `scripts/pr-images.sh <PR> <png>...` (gh can't upload images). A ticket left unfinished goes up as a draft with "Part of #N" and its open criteria listed. Done when the PR is open; merging is the coordinator's (`docs/agents/reviewer.md`).

## In an integration run

In an integration run (`/implement-spec`), tickets land on an integration branch instead of main (`docs/agents/reviewer.md`, "Integration branches"). The coordinator names the ticket, the worktree, the branch and the integration branch; the steps above change here:

- **Worktree.** Start the branch from the integration branch: `git fetch -q && git switch -C <branch> origin/<integration>`. Done when `git merge-base --is-ancestor origin/<integration> HEAD` succeeds; if it fails, switch again.
- **"vs main"** in a brief means vs the integration branch, the null check's reference among them (ADR 0008, "Null check").
- **No PR.** What a PR would carry goes in the commit messages' bodies, where the integration PR's reviewer reads it: measurements, null and size numbers, what only a host shows, each criterion with no red test and why. CPU scenes before/after are `scripts/check.sh cpu origin/<integration>`'s output, load average included. Issues and the integration branch are the coordinator's.
- **Merge, then push**, in place of the rebase: `git fetch -q && git merge origin/<integration>`, keeping both sides where parallel tickets added beside each other; the focused tests pass again; then `git push -u origin <branch>`. Done when the push succeeds.
- **Report** sparsely, in 15 lines at most: branch, head SHA, commits, each acceptance criterion and its test (or why none), and what is left open or needs the user. A brief that is ambiguous, or a quality bar the ticket can't meet, is reported at once instead of worked around.

## Running several builders

Each builder gets its own worktree and touches different files where the plan allows. On one 8-core Mac, run at most three: past that, builds and tests slow every builder down several times over. The gate holds a fourth commit or push until one of three finishes (`.githooks/gate.sh`), but a fourth builder still compiles and tests alongside them. The worktrees share the main checkout's `.deps`, dependencies' build folders included (Catch2, the CLAP extensions): two builds that reach them at once can truncate Ninja's log there (`ninja: warning: premature end of file`), and the next build recompiles those dependencies. That costs time, not correctness. A build for other architectures than the universal `arm64;x86_64` would rebuild them for its own alone and break the others' links, so CMake refuses it on a shared `.deps`; such a scratch build takes a folder of its own, as the sanitizer stages do (`scripts/check.sh`, `sanitizer_deps`).

`refs/stash` is one stack shared by every worktree, so one builder's `git stash pop` can take another's changes. A builder sets work aside with a WIP commit on its own branch.

## A Staple reskin slice

The handoff (`docs/staple-handoff/`) decides how it looks; spec #1 decides what it does, and its "Staple reskin" list names where they differ.

- **Render the prototype** to compare against images, not markup: `python3 -m http.server -d docs/staple-handoff/prototype 8765`, then open `http://localhost:8765/Main.dc.html` (or `DesignSystem.dc.html` for single controls). For a PNG at the editor's size: Chrome `--headless=new --window-size=1200,760 --force-device-scale-factor=2 --screenshot=<file.png> <url>`.
- **Render the editor** with `scripts/check.sh screens <dir>`, which writes every hidden `[.screens]` test's PNGs into `<dir>` (at the editor's size, mostly at 2x), and check them side by side with the prototype area by area. The PR attaches both (`scripts/pr-images.sh`). A new render goes through `harness::writeSnapshot` (`tests/plugin/EditorHarness.h`) with the `[.screens]` tag.
- Every colour and size comes from `plugin/staple/Tokens.h`; the editor has one LookAndFeel, `staple::LookAndFeel`.
