# Performance never costs sound

**Status:** Accepted (#164, #165): the quality bar every slice of the Performance & hardening batch meets, and every later performance change after it. Amended 2026-10-12 (#202): the exact rule holds per platform, and each compiler's default contraction is allowed ("SIMD stays portable").

The Performance & hardening batch (#164) cuts the plugin's CPU on the audio thread and in the editor, its per-instance RAM and its binary size, and hardens it against bad input. Each of those invites a trade: a cheaper detector that approximates `log10`, a cascade in single precision, a coarser Analyzer, a tolerance nudged up so a faster design still passes. An EQ sells on its sound, and every one of those trades is audible or measurable by a user before it is visible in a profiler. So the batch was grilled on 2026-10-11 with one rule, and this ADR records it so later perf work and its reviews are held to it: performance never costs sound.

Concretely, for every slice that touches the Engine:

- **Null check.** `scripts/check.sh null <ref>` (the `null` stage, built by #167) renders fixed scenes through the Engine at `<ref>` and at the working tree and prints the worst deviation per scene in dBFS, or `exact` when the outputs are bit-identical. A slice stays at or below **−120 dBFS** against `main` (in an integration run, against the integration branch it builds on) on every render scene. A slice that only skips or caches work it would otherwise repeat (idle Dynamic Bands, a redesign that nothing changed, settings re-applied to untouched slots) is **exact**: it has no reason to change a bit.
- **No tolerance loosened.** No response, click or tolerance table in an existing test is widened, and the bands in `docs/dsp/filter-design.md` stay as they are. Block-size invariance stays exact: the output is the same, sample for sample, however the host cuts the audio into blocks (`docs/performance.md`, "Sample rates and block sizes"). The real-time safety tests still pass.
- **No fast-math, no approximations.** The build keeps strict IEEE semantics (no `-ffast-math`, `/fp:fast` or their parts), and no transcendental function (`log10`, `exp`, `pow`, `sin`, `cos`, `tan`) is replaced by an approximation. Work is saved by doing it less often (once per 16-sample run, once per block, once per change), not by doing it less exactly.
- **Double filter state and decramped designs stay.** Filter state is double precision, and every Shape keeps its decramped, matched-response design (ADR 0001, `docs/dsp/filter-design.md`, "Method"). A cheaper bilinear or cookbook design is not a perf slice; it is a sound change.
- **No feature, Shape or Analyzer resolution cut.** Nothing a user can hear or see is removed or coarsened to save CPU: no Shape, Stereo Placement or Dynamics option, and no Analyzer resolution, speed or range. The Analyzer's FIFOs may shrink to what the editor can show, but what it shows doesn't change.
- **SIMD stays portable.** Two-lane double only (NEON on arm64, SSE2 on x64) with a scalar fallback that gives the same result; no AVX, no FMA intrinsics, no `-march`, no runtime dispatch. Each compiler's default floating-point contraction is allowed: clang's `-ffp-contract=on` fuses `a * b + c` within one expression into a fused multiply-add on arm64, while x64 (SSE2 has no fused multiply-add) and MSVC's `/fp:precise` fuse nothing. A fused multiply-add rounds differently from a multiply and an add, so the vector and scalar paths must contract identically: the same expression for both, with the compiler's own operators rather than intrinsics (`engine/src/Double2.h`).
- **Exact per platform.** The null check's −120 dBFS and "exact" hold between two builds on one platform, which is what `scripts/check.sh null` measures: it builds both on the machine it runs on. arm64 and x64 builds may differ from each other in the last bits, by contraction alone.

A slice that cannot meet the bar stops and reports instead of relaxing it: a reviewer decision, not a builder's. A red null check or a loosened tolerance is a review failure.

## Non-finite input

Hardening gave the spec one line, decided here and recorded in spec #1 under its real-time decisions: **"A non-finite input sample never reaches the output; a Band recovers within one 16-sample run."** A NaN or infinite sample from the host or an upstream plugin today enters a Band's biquad state and stays there until the next prepare, and takes its detector and the Analyzer and Output Level taps with it. The fix (#184) checks and clears state once per run and replaces non-finite output samples with 0, so it is subject to this bar too: exact on finite input, block-size invariance intact.

## Considered Options

- **A per-slice budget for sound change** (say −100 dBFS, or a relative tolerance on the response tests). Rejected: a budget invites spending it, and a chain of slices each within budget drifts. −120 dBFS is 20 dB under a 16-bit noise floor and a few ulps of a single-precision output sample, what a change in summation order produces; anything above it is a change in the design.
- **Approximate transcendentals with a measured error bound.** Rejected: the detector's `log10` and the designs' `exp` and `tan` set Threshold, Frequency and Q, and an approximation's error lands in the response. Calling them less often gets the same saving.
- **Single-precision filter state where the sections are "benign".** Rejected: steep Bells reach section Qs in the hundreds (ADR 0007), where single precision rings and drifts, and deciding per section which are benign is exactly the kind of judgement that goes wrong in review.
- **`-ffp-contract=off` everywhere, for one result on every machine** (#201, #202). Rejected: it gives up arm64's fused multiply-adds in every filter step, which costs arm64 speed, and trying it moved the null baseline to about −117 dBFS on two 24-Band scenes, a sound change of its own made for a property no user hears and no check measures. A cross-architecture null check would be needed to hold it, and there is none.
- **Letting the ceilings decide.** Rejected: `scripts/check.sh cpu` measures cost, not sound. The two checks are separate on purpose: a slice must pass both, and neither may buy the other.

## Consequences

- Every perf PR against the Engine carries its `null` result (per scene, "exact" or the worst dBFS) and its before and after CPU scenes in its description or commit bodies, so the reviewer reads the evidence instead of trusting the diff.
- Slices that skip or cache work are reviewed against "exact", not "−120 dBFS": a non-zero deviation from one of them means it changed the order or amount of arithmetic, which needs explaining.
- The Engine's remaining headroom comes from doing less: fewer redesigns, fewer per-sample interpolations, fewer `log10` calls, smaller FIFOs, stripped binaries. Where that is not enough, the ceilings in `docs/performance.md` are what move, not the sound.
- Reviews of later perf work cite this ADR by number. A change that needs to break one of these rules is a new ADR that supersedes this one for that rule, with a listening and null-check argument of its own.
