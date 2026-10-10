# Coding standards

Judgement calls for review. Mechanical rules live in `scripts/check.sh` and the pre-commit hook instead.

## Tests reach what they claim to test

A test's inputs must cover the region its name and comments claim. Check sweep bounds and step counts against that region, and make sure a test signal actually has content there.

- **Sweeps:** a sweep "across the hold at 0.95 × Nyquist" must run past 0.95 × Nyquist at every sample rate it is run at, with its step count computed from the range, not fixed.
- **Red without the change:** a test added with a fix or a guard fails when that code is taken out. Untouched samples, a property the code can't affect, or a check the code passes by construction make it pass either way. The PR shows the failing run as evidence, or the reviewer asks for one.
- **Paths on every OS:** a path a test compares or prints is written with `/` on every OS (`replaceCharacter ('\\', '/')` on a relative path), so a test that passes on macOS doesn't fail on Windows CI.
- **Fixtures near Nyquist:** a fixture that shapes a test signal (a band-limit, a reference filter) is exact over the whole range it is used in. The Engine's own Shapes are inexact near Nyquist (`docs/dsp/filter-design.md`, "Test tolerances"), so a test band-limits with them only where they are accurate, or at a sample rate where they are (96 kHz for a 20 kHz High Cut).
- **Editor behaviour:** the editor, the EQ display and their keys are testable through `tests/plugin/EditorHarness.h`, which opens the editor in a window of its own; a test of either goes through it rather than being skipped.

## Comments and docs state current behaviour

A comment or doc says what the code does now and why. History (what changed, what the limits used to be, which ticket changed them) belongs in commit messages. A pointer to an open ticket for planned work stays, as in `Bell ignores Slope until Bell Slope (#19)`.
