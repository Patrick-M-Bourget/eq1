# eq1

Real-time EQ plugin (VST3, AU, AAX, CLAP and Standalone) for macOS Universal and Windows x64, built on JUCE 9. Vocabulary is in `GLOSSARY.md`; design decisions are in `docs/adr/`.

## Layout

- `engine/` is the DSP Engine: plain C++ with no dependency on JUCE.
- `plugin/` is the Plugin Shell: the JUCE layer that maps host parameters to Engine settings.
- `tests/engine/` tests the Engine through its public interface; `tests/plugin/` drives the Plugin Shell like a host; `tests/host/` loads the built plugins through their format wrappers, as a DAW does.

## Build and test

`scripts/check.sh` runs everything CI runs:
- checks that every doc section cited in code exists;
- builds every format (macOS Universal or Windows x64);
- runs the Engine and Plugin Shell tests;
- runs the Engine tests under ThreadSanitizer (macOS only);
- validates the plugins with pluginval, auval and clap-validator, and checks Sidechain routing through the VST3 and AU wrappers.

Run one part with `scripts/check.sh docs|build|test|tsan|validate`. CMake fetches the dependencies (JUCE, clap-juce-extensions, Catch2) into `.deps/`. The validators are fetched there too, with an authenticated `gh`.

`scripts/ci-timings.sh <run-id> [attempt]` prints how long each CI job and step took, and why a job never started.

Run `scripts/install-hooks.sh` once per clone: its pre-commit hook blocks a commit whose build or tests (all but the slow response grids) fail.

Plugins land in `build/plugin/eq1_artefacts/Release/`. AAX is built unsigned and can only be loaded in Pro Tools Developer.

Filter design notes are in `docs/dsp/filter-design.md`, with a Python lab for trying designs in `tools/filter-lab/`.
