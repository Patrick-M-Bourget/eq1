# Performance and robustness

## CPU budget

`tests/cpu/CpuBudget.cpp` (`scripts/check.sh cpu`, its own CI step) measures the Engine's load in each CPU scene, stereo, at 48 and 96 kHz, on noise that is loud for 100 ms of every 200 so Dynamic Bands keep moving and redesigning their filters: the median of five 4-second runs, in percent of real time. The scenes are a list of `Scene`s in `CpuBudget.cpp`, built on the shared 24-Band scenes in `tests/engine/Measure.h`; each prints its load, so a change shows what it did to each. Only the 24 Dynamic Bells scene (Bells across the spectrum, every Detection Range and Threshold kind, one metered, 512-sample blocks) has a ceiling; the others have none yet.

| CPU scene | Ceiling 48 / 96 kHz | Apple M3 48 kHz | 96 kHz | CI Windows x64 48 kHz | 96 kHz | CI macOS (arm64) 48 kHz | 96 kHz |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 24 Dynamic Bells | 35% / 60% | 3.8% | 7.4% | 9.6–11.0% | 18.1–19.1% | 5.9% | 14.6% |
| 24 Bells | — | 0.5% | 0.9% | pending | pending | pending | pending |
| 1 Bell | — | 0.1% | 0.2% | pending | pending | pending | pending |
| 24 Brickwall Cuts | — | 5.3% | 10.5% | pending | pending | pending | pending |
| 24 Bells, Output Gain and Pan | — | 0.5% | 1.0% | pending | pending | pending | pending |
| 24 Dynamic Bells, 32-sample blocks | — | 3.8% | 7.5% | pending | pending | pending | pending |
| 24 Dynamic Bells on silence | — | 3.3% | 6.6% | pending | pending | pending | pending |
| 24 Dynamic Bells on subnormal input | — | 2.3% | 4.6–9.4% | pending | pending | pending | pending |

The 24 Dynamic Bells row was measured 2026-10-09, the other rows on this Mac 2026-10-11 with other builds running (the 24 Dynamic Bells scene read the same as on 2026-10-09, so the rest are taken as quiet too; the subnormal input number varied between runs). The CI columns are GitHub's `windows-latest` and `macos-latest` runners; the new scenes' CI numbers (pending) come from the first CI run's job summary, where `scripts/check.sh cpu` writes the scene lines. The ceilings are about three times the slowest runner's worst run: high enough that runner noise doesn't fail the check, low enough to fail on a regression that multiplies the cost. They are absolute, not relative to a baseline.

The same run fails if the 24 Dynamic Bells on silence (timed after 10 s of nothing, when every filter has rung down) or on input made of subnormal numbers cost more than 1.5 times the 24 Dynamic Bells on music. That catches arithmetic on subnormal numbers, many times slower than normal on x64; on arm64 it runs at full speed, so the Windows x64 run is the one that would catch it. It also shows the Dynamic Bands at rest: a Dynamic Band whose detector has gone quiet stops redesigning its filter within a few seconds (`docs/dsp/filter-design.md`, "Rest"), so silence costs about what the 24 static Bells do.

## Null check

`scripts/check.sh null <ref>` shows what a change did to the Engine's output, sample for sample, against any git ref (`origin/main` for a PR). It builds `<ref>`'s render tool (`tests/render/RenderTool.cpp`, built with the tests and linked against the Engine only) in a temporary worktree under `build/null`, with the same compiler and flags as this tree and without the Plugin Shell, renders the render scenes through both Engines, and prints each scene's worst sample deviation in dBFS at each sample rate, or `exact` when the two renders are bit-identical. It fails above −120 dBFS, when a scene's two renders differ in length, or when `<ref>` has a scene this tree hasn't; a scene only this tree has is `new` and passes. A change that only skips or caches work is held to `exact` by review (ADR 0008). The worktree and its build are removed on exit; the renders stay in `build/null/current` and `build/null/ref`.

The render scenes (`tests/render/RenderScenes.h`) are data the tool and the comparison share, so a scene added there is rendered and compared on both sides. Each is stereo, one second of full-scale noise bursts (100 ms loud, 100 ms at −34 dB, seeded, so every run renders the same input) through 97-sample blocks, not a multiple of the Engine's 16-sample run, at 44.1, 96 and 192 kHz: one Band of every Shape, Low Cut and High Cut with Brickwall too; a Bell in every Stereo Placement; a Dynamic Bell with the Band Detection Range and Auto Threshold, with the Free Detection Range and a manual Threshold, and with the Sidechain as Detection Source (fed bursts offset from the main input's); 24 Bells and the CPU budget's 24 Dynamic Bells; a glide of Frequency and Gain, a crossfade from a Bell to a Low Cut, Solo, Detection Audition and Global Bypass switched on a quarter of a second in; Output Gain with Pan and Phase Invert, Pan in Mid/Side, Auto Gain and Gain Scale. `tests/engine/RenderScenesTest.cpp` keeps the list covering all of this and every render repeatable.

## Paint time

`tests/plugin/PaintTimeTest.cpp` (`scripts/check.sh paint`) measures how long the whole editor takes to draw a busy frame: 1200 × 760 rendered into an image at 2× (`createComponentSnapshot` with scale 2), with 24 Dynamic Bells across the spectrum, one selected, the Analyzer showing Pre, Post, Sidechain and Peak Hold on spectra fed with noise on the main input and the Sidechain, and the Output Meter shown. It prints the median of 20 frames and fails when that median is above its ceiling of 200 ms, about 3× the median measured with the Staple display (the grid, the Analyzer, every Band's curve with its fill, the sum's halo and the edge fades). Every later change to the editor's drawing stays under it.

The test is tagged `[.paint]`, hidden from the normal run so its timing never flakes it. `scripts/check.sh paint` builds and runs it, and is local only: CI doesn't run it. Like the CPU budget, it measures nothing on a Mac busy with other work (a load average above half the cores) and exits 3, which `scripts/check.sh` with no stage skips. To time it anyway, run `build/tests/eq1_plugin_tests "[paint]"`.

| Machine (quiet) | Median frame |
| --- | --- |
| Apple M3, before the Staple display (`ef8a283`) | 49.1 ms |
| Apple M3, with the Staple display (`c601e21`) | 64.2 ms |

Measured 2026-10-10. On a machine busy with other builds the median reached 125 to 160 ms at a load average above 80: the ceiling allows for some load, but not that much.

**Idle frame, Analyzer on.** The same file's `Idle frame, Analyzer on` test, run with the paint time, measures what a frame costs when nothing changes but the spectra, which is every frame with the Analyzer on: the display alone at 2× with 24 Bells, one selected, the Analyzer showing Pre and Post on silence. It prints the median of 20 paints of the display and the message thread's CPU per tick over 2 s of ticks (a test window never paints on its own, so that is the tick's own work: the Settings read, the spectra updated, and whatever the frame places). The display keeps its Band curves, fills, wash and sum as paths (`display::CurveCache`) and places its handles, reading out what changed, only when the Bands shown or the Display Range change, so an Analyzer frame draws the spectra and strokes the paths it has.

| Machine | Display paint | Tick CPU |
| --- | --- | --- |
| Apple M3, load average about 65 from parallel builds, before the curve cache (`7f175d4`) | 34 to 43 ms | 0.63 to 0.66 ms |
| Apple M3, the same load, with it (`8ccf0bc`) | 15 to 28 ms | 0.11 to 0.40 ms |

Measured 2026-10-12, three runs each, back to back; the whole editor's busy frame above went from 171 to 196 ms to 69 to 113 ms in the same runs. Quiet-machine figures are still to be taken.

## Footprint

`scripts/check.sh size` measures what ships. It builds the shipping configuration (Release, `EQ1_LTO=ON`, Universal) in `build-size` and prints three reports; local only, CI doesn't run it. On Windows it prints the Engine heap only.

**Binary size.** Each format's binary per architecture, unstripped and as `strip -x` leaves it, and the universal file as built. Measured 2026-10-11 on `perf-integration` (`749fc3a`), Apple clang 17:

| Format | arm64 | arm64 stripped | x86_64 | x86_64 stripped | Universal |
| --- | --- | --- | --- | --- | --- |
| VST3 | 8,374,080 | 6,663,360 | 9,100,272 | 7,395,232 | 17,499,968 |
| AU | 8,392,224 | 6,715,408 | 9,002,808 | 7,344,544 | 17,403,424 |
| AAX | 8,400,496 | 6,698,624 | 9,000,120 | 7,317,072 | 17,411,696 |
| CLAP | 8,395,728 | 6,664,496 | 8,987,216 | 7,275,344 | 17,390,544 |
| Standalone | 8,279,728 | 6,439,208 | 8,856,640 | 7,037,640 | 17,143,472 |

**Code by library.** The VST3's `__text` attributed by `scripts/code-by-library.py`: each symbol is looked up in the normal build's objects (the LTO link's own map attributes everything to one `lto.o`), and one no object or several define (template instances, LTO-renamed functions) goes by the namespace in its name. Same build; the arm64 `__text` is 5,078,020 bytes, the x86_64 one 5,752,032:

| Library | arm64 | x86_64 |
| --- | --- | --- |
| juce_gui_basics | 894,248 | 979,168 |
| eq1 Plugin Shell | 870,604 | 923,904 |
| juce_graphics | 853,384 | 989,664 |
| harfbuzz | 670,456 | 746,640 |
| codec: opus | 455,544 | 486,224 |
| codec: libwebp | 442,372 | 651,216 |
| juce_core | 197,804 | 214,288 |
| juce_audio_plugin_client | 108,088 | 117,648 |
| lunasvg | 98,424 | 114,464 |
| codec: libjpg | 94,200 | 108,256 |
| static initialisers | 72,660 | 56,160 |
| eq1 Engine | 43,708 | 62,256 |
| JUCE templates (by namespace) | 36,040 | 39,216 |
| juce_data_structures | 35,964 | 36,336 |
| juce_audio_processors_headless | 35,452 | 45,232 |
| juce_audio_processors | 28,836 | 27,552 |
| codec: zlib | 27,172 | 30,720 |
| codec: libpng | 24,892 | 28,688 |
| juce_audio_formats | 22,788 | 28,944 |
| juce_events | 18,324 | 19,024 |
| libc++ templates | 16,932 | 17,888 |
| juce_dsp | 9,656 | 9,504 |
| juce_audio_basics | 9,236 | 10,496 |
| other | 5,660 | 3,648 |
| codec: ogg | 2,760 | 2,704 |
| juce_gui_extra | 2,084 | 1,456 |
| sheenbidi | 732 | 736 |

The codecs eq1 never decodes (opus, libwebp, libjpg, libpng, zlib, ogg) and lunasvg are about 1.1 MB of the arm64 code; harfbuzz another 0.67 MB.

**Engine heap.** `tests/engine/FootprintTest.cpp` (tagged `[footprint]`, in the normal test run) constructs one `Engine`, prepares it stereo, and reads from `eq1::test::AllocationGuard` what the heap holds: the bytes the allocator gave out, less what was freed meanwhile, in the allocator's own block sizes (`malloc_size`, `_msize`). It fails above its ceilings, `heapCeilingBytes48k` and `heapCeilingBytes192k`, set about 5% above the figures measured here; a change that shrinks the Engine lowers them.

| Sample rate | Heap per stereo Engine | Ceiling |
| --- | --- | --- |
| 48 kHz | 443,776 bytes | 466,000 |
| 192 kHz | 787,840 bytes | 827,000 |

Measured 2026-10-12 on an Apple M3 (`177-buffers-sized-to-need`). Most of it is the Bands' and detectors' state. The Analyzer's three taps and the Output Level's two channels hold **114,880 bytes** together at 48 kHz stereo, against a ceiling of 200,000 in the same test (`tapsCeilingBytes48k`), which `scripts/check.sh size` prints beside the heap:

- An Analyzer tap (`engine/src/AnalysisFifo.h`) is a ring of the smallest power of two covering 150 ms at the prepared sample rate: 8,192 samples at 44.1 and 48 kHz, 16,384 at 96 kHz, 32,768 at 192 kHz, indexed with a mask. A ring is never freed or moved while the Engine lives (a prepare that needs a bigger one adds it and keeps the old one), so the editor reads through any re-prepare. `Engine::analysisCapacity()` tells the editor how big its read buffer is.
- An Output Level tap (`engine/src/OutputLevelTap.h`) keeps its 300 ms RMS window as per-run (16-sample) sums of squares in double, rounded up to whole runs: 900 doubles at 48 kHz. A second sum, started afresh each window, replaces the running one once it has covered a whole window, so rounding never builds up and the audio thread never resums the window. The two taps are allocated once, for stereo, and the live channel count is atomic, so a mono/stereo re-prepare frees nothing under the Output Meter's reads.

## Subnormal numbers

`Engine::process()` flushes subnormal numbers to zero for its own duration (`engine/src/NoSubnormals.h`), whatever the host's floating-point mode, and restores that mode afterwards. Without it, a filter ringing down after the input stops outputs subnormal floats for seconds, and its double-precision state passes through the subnormal range after that.

## Identity sections

A Band's cascade keeps a fixed number of biquads per structure, so glides can interpolate; some of them are exactly the identity: an unsplit resonant shelf's second biquad (`docs/dsp/filter-design.md`, "Resonant shelves"), whose matched poles are cancelled by equal zeros. A section that is exactly the identity (`b0 = 1`, `b1 = a1`, `b2 = a2`, `engine/src/Biquad.h`) at both ends of a run, so at every interpolated position in it, and whose state is clear on every line it runs on, is skipped for the piece: its samples and state stay exactly what running it would leave, so the output is bit-identical (`docs/adr/0008-performance-never-costs-sound.md`), and the section count doesn't change. One whose state still holds what it filtered before it became the identity runs until that has drained: the plain identity drains in two samples; cancelled poles ring down at their own rate, and on arm64, where clang fuses the step's multiply-adds, the drain completes only in digital silence, since `a1 * x - a1 * y` fused leaves a rounding residual (about 1e-17) while a signal plays. A Bell at 0 dB is designed as a matched section whose numerator equals its denominator in the analog domain, which the three-point match reproduces only to about 1e-14, so it is not an exact identity and is not skipped. `Engine::sectionsSkipped()` counts the skipped passes, for tests (`tests/engine/IdentitySectionsTest.cpp`).

## Non-finite input

A NaN or infinite input sample, from the host or an upstream plugin, never reaches the output (`docs/adr/0008-performance-never-costs-sound.md`, "Non-finite input"): `Engine::process()` replaces any non-finite output sample with 0, after the output stage and Global Bypass, and the Analyzer taps push 0 in its place. Every Band's filter chain (a detector's Detection Range filters and the Solo region included) checks its state once per run, at the run's start, and clears any section that isn't finite, starting afresh at that run's design with no glide from the filter it lost; a detector that heard one starts afresh at the run's end, Auto Threshold included, and its Band rests at its Gain over the next run. So a Band recovers within one 16-sample run, and from the next run on the output is what a freshly prepared Engine plays on the same material (`tests/engine/NonFiniteInputTest.cpp`). On finite input the output is bit-identical to before the checks, which cost one comparison per section per run, six per detector per run and one per output sample.

## Sample rates and block sizes

eq1 is checked at 44.1, 48, 88.2, 96, 176.4 and 192 kHz, and at block sizes from 1 sample up, changing from call to call. The output is the same, sample for sample, however the host cuts the audio into blocks, Dynamic Bands included. The Engine runs on a grid of 16-sample runs that starts at `prepare()` and carries across `process()` calls: a block ending mid-run leaves the run open for the next one. Everything timed per run (a Band's re-design and glides, Auto Attack, Auto Threshold) happens once per grid run, and a Dynamic Band's filter over a run follows only what its detector heard in earlier runs, so it reacts up to one run (16 samples, about 0.33 ms at 48 kHz) after the sound, with no added latency. Settings still arrive per block.
