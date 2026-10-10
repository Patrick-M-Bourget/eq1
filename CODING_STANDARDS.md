# Coding standards

Judgement calls for review. Mechanical rules live in `scripts/check.sh` and the pre-commit hook instead.

## Tests reach what they claim to test

A test's inputs must cover the region its name and comments claim. Check sweep bounds and step counts against that region, and make sure a test signal actually has content there.

- **Sweeps:** a sweep "across the hold at 0.95 × Nyquist" must run past 0.95 × Nyquist at every sample rate it is run at, with its step count computed from the range, not fixed.
- **Red without the change:** a test added with a fix or a guard fails when that code is taken out. Untouched samples, a property the code can't affect, or a check the code passes by construction make it pass either way. The PR shows the failing run as evidence, or the reviewer asks for one.
- **Paths on every OS:** a path a test compares or prints is written with `/` on every OS (`replaceCharacter ('\\', '/')` on a relative path), so a test that passes on macOS doesn't fail on Windows CI.
- **Fixtures near Nyquist:** a fixture that shapes a test signal (a band-limit, a reference filter) is exact over the whole range it is used in. The Engine's own Shapes are inexact near Nyquist (`docs/dsp/filter-design.md`, "Test tolerances"), so a test band-limits with them only where they are accurate, or at a sample rate where they are (96 kHz for a 20 kHz High Cut).
- **Editor behaviour:** the editor, the EQ display and their keys are testable through `tests/plugin/EditorHarness.h`, which opens the editor in a window of its own; a test of either goes through it rather than being skipped. A check that depends on the mouse pointer sends the pointer again after the editor settles: JUCE's Desktop timer can move it to the machine's real mouse meanwhile. A wait on the pointer's rest (the Hover Card's) goes through `OpenEditor::restUntil`, which keeps sending it; one move and a fixed `settle` passes on macOS and fails on Windows CI. A control's own hover and press are the exception: a test window isn't in front, so the pointer's events never reach it and `isMouseOver` reads the real mouse. Such a test calls the control's mouse handlers (or `paintButton (g, highlighted, down)`) directly and counts the repaints asked for, as `HoverKit` in `StapleControlsTest.cpp` does.

## Accessibility is behaviour

What a control reports to a screen reader (its handler, role, title, value) is behaviour, even in a refactor: a change to it is named in the PR and pinned by a test. Moving every chip to one handler once gave the action buttons a value equal to their title, read twice.

## Comments and docs state current behaviour

A comment or doc says what the code does now and why. History (what changed, what the limits used to be, which ticket changed them) belongs in commit messages. A pointer to an open ticket for planned work stays, as in `Bell ignores Slope until Bell Slope (#19)`.

## Shared behaviour lives once

A control behaviour (drag and Shift-drag, double-click reset, hover and press light, enabled dimming, tweens, overlays, accessibility handlers) or a test helper (snapshots, synthetic mouse events, layouts) that a second component needs moves into `plugin/staple/` or `tests/plugin/EditorHarness.h`, and both call it. A second copy written beside the first drifts: two of five copied drag handlers snapped back after a double-click.
