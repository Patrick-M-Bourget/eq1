# eq1

Real-time EQ plugin (VST3/AU/AAX/CLAP) modeled on FabFilter Pro-Q 4, built with JUCE 9. Use the vocabulary in `GLOSSARY.md`; respect the decisions in `docs/adr/`.

Checks: `scripts/check.sh` (its header lists them); run a few tests with `scripts/check.sh focus <regex>`.

Filter design (analog targets, decramping, test tolerances): `docs/dsp/filter-design.md`. Try a design in `tools/filter-lab/filterlab.py` before writing C++.

## Agent skills

### Issue tracker

Issues and specs live in GitHub Issues for Patrick-M-Bourget/eq1, via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.

### Feature workflow

New features: `/grilling` → amend the spec (#1) → `/domain-modeling` for new terms → slice tickets (`needs-triage`) → triage writes each `## Agent Brief`. Tickets come last, never straight from analysis.

### Worktrees

The main checkout stays on `main`. Work on a branch in its own worktree, with its own `build/`: `git worktree add ../eq1-<ticket> -b <branch> origin/main`. A hook (`.claude/hooks/one-branch-per-worktree.sh`) blocks switching branches in the main checkout. An approved change that isn't applied at once, such as a retro's, goes on a branch or a draft PR, where other sessions can see it.
