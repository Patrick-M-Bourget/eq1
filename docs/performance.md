# Performance and robustness

## CPU budget

`tests/cpu/CpuBudget.cpp` (`scripts/check.sh cpu`, its own CI step) measures the Engine's load in each CPU scene, stereo, at 48 and 96 kHz, on noise that is loud for 100 ms of every 200 so Dynamic Bands keep moving and redesigning their filters: the median of five 4-second runs, in percent of real time. The scenes are a list of `Scene`s in `CpuBudget.cpp`, built on the shared 24-Band scenes in `tests/engine/Measure.h`; each prints its load, so a change shows what it did to each. Two scenes have a ceiling: the 24 Dynamic Bells (Bells across the spectrum, every Detection Range and Threshold kind, one metered, 512-sample blocks) and the same with Band 1's Gain automated, whose settings are published before every block, as the plugin publishes them while Host Automation moves a parameter, so the cost of applying settings is measured with the block. The others have none yet.

| CPU scene | Ceiling 48 / 96 kHz | Apple M3 48 kHz | 96 kHz | CI Windows x64 48 kHz | 96 kHz | CI macOS (arm64) 48 kHz | 96 kHz |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 24 Dynamic Bells | 35% / 60% | 3.8% | 7.4% | 9.6–11.0% | 18.1–19.1% | 5.9% | 14.6% |
| 24 Bells | — | 0.5% | 0.9% | pending | pending | pending | pending |
| 1 Bell | — | 0.1% | 0.2% | pending | pending | pending | pending |
| 24 Brickwall Cuts | — | 5.3% | 10.5% | pending | pending | pending | pending |
| 24 Bells, Output Gain and Pan | — | 0.5% | 1.0% | pending | pending | pending | pending |
| 24 Dynamic Bells, 32-sample blocks | — | 3.8% | 7.5% | pending | pending | pending | pending |
| 24 Dynamic Bells, Gain automated | 35% / 60% | 1.02 × 24 Dynamic Bells | 1.02 × | pending | pending | pending | pending |
| 24 Dynamic Bells on silence | — | 3.3% | 6.6% | pending | pending | pending | pending |
| 24 Dynamic Bells on subnormal input | — | 2.3% | 4.6–9.4% | pending | pending | pending | pending |

The 24 Dynamic Bells row was measured 2026-10-09, the other rows on this Mac 2026-10-11 with other builds running (the 24 Dynamic Bells scene read the same as on 2026-10-09, so the rest are taken as quiet too; the subnormal input number varied between runs). The CI columns are GitHub's `windows-latest` and `macos-latest` runners; the new scenes' CI numbers (pending) come from the first CI run's job summary, where `scripts/check.sh cpu` writes the scene lines. The ceilings are about three times the slowest runner's worst run: high enough that runner noise doesn't fail the check, low enough to fail on a regression that multiplies the cost. They are absolute, not relative to a baseline. The Gain automated scene was measured 2026-10-12 on this Mac under a load average of 35 to 110, so only relative to the 24 Dynamic Bells in the same run (7 alternating runs at 48 and 96 kHz, median ratio 1.02); its ceiling applies the same margin to the slowest runner's 24 Dynamic Bells times that ratio (11.0% and 19.1% × 1.02 × 3), which rounds to the 24 Dynamic Bells' own 35% / 60%. Its CI numbers come from the first CI run.

The same run fails if the 24 Dynamic Bells on silence (timed after 10 s of nothing, when every filter has rung down) or on input made of subnormal numbers cost more than 1.5 times the 24 Dynamic Bells on music. That catches arithmetic on subnormal numbers, many times slower than normal on x64; on arm64 it runs at full speed, so the Windows x64 run is the one that would catch it. It also shows the Dynamic Bands at rest: a Dynamic Band whose detector has gone quiet stops redesigning its filter within a few seconds (`docs/dsp/filter-design.md`, "Rest"), so silence costs about what the 24 static Bells do.

## CPU A/B

`scripts/check.sh cpu <ref>` says what a change did to each CPU scene's cost against any git ref (`origin/main` for a PR, the integration branch in an integration run), on a machine busy with other builds too. Absolute loads on a busy Mac or a CI runner vary about 2× from run to run; the ratio of two builds timed side by side doesn't, because load hits both alike. It builds `<ref>`'s Engine in a temporary worktree under `build/cpu-ab`, the way the null check builds its reference, with this tree's CPU scenes copied over (`tests/cpu/` and `tests/engine/Measure.h`), so both sides run the same scenes and only the Engine differs. Then, for 7 rounds, it runs every scene at 48 kHz through one build and then the other for about a second each, swapping which goes first every round. Each run is a fresh Engine timed by its thread's CPU time, so time spent waiting for a core isn't counted.

It prints the load average before and after, then per scene the median after/before ratio (this tree's load over `<ref>`'s: above 1 is slower), the lowest and highest round, how many rounds went up and down, and a verdict: **slower** or **faster** when the median is at least 3% from 1.0 and at least 6 of the 7 rounds went that way, **no clear change** otherwise. The round count, run length, sample rate and thresholds are named at the top of the stage in `scripts/check.sh`; the verdict is `scripts/cpu-ab.awk`, tested by `scripts/cpu-ab.test.sh` (`scripts/check.sh hooks`). It is a report, not a gate: it exits 0 whatever the verdicts and whatever the load, and fails only when a build fails, this tree's scenes among them (a scene that uses something `<ref>`'s Engine lacks can't be compared). Each run's loads stay in `build/cpu-ab/loads.tsv`. A perf PR carries its output, load average included, as its before and after CPU scenes (ADR 0008).

Two runs on this Mac, 2026-10-12, against `perf2-integration` with two other builds running: this tree unchanged (load average 111 before, 75 after) read "no clear change" on every scene, medians 0.975 to 1.029; with a busy-wait added to every Band's detector (not committed) every scene read "slower", medians 1.21 to 11.7. A median 2% off 1.0 with 6 rounds agreeing showed up on the unchanged tree, so differences below the 3% threshold are noise, not findings.

## Null check

`scripts/check.sh null <ref>` shows what a change did to the Engine's output, sample for sample, against any git ref (`origin/main` for a PR). It builds `<ref>`'s render tool (`tests/render/RenderTool.cpp`, built with the tests and linked against the Engine only) in a temporary worktree under `build/null`, with the same compiler and flags as this tree and without the Plugin Shell, renders the render scenes through both Engines, and prints each scene's worst sample deviation in dBFS at each sample rate, or `exact` when the two renders are bit-identical. A scene with a Metered Band also renders its Detection Level readings (one per block, read as the editor reads them) and prints their worst deviation in dB on a line of its own, so a change to the detector shows in what the meter reports, not only in the output. It fails above −120 dBFS, when a scene's two renders differ in length, or when `<ref>` has a scene this tree hasn't; a scene only this tree has is `new` and passes. A change that only skips or caches work is held to `exact` by review (ADR 0008). Both builds run on the machine the check runs on, so it compares within one platform: an arm64 and an x64 build may differ in the last bits, where clang fuses `a * b + c` on arm64 and nothing fuses on x64 (ADR 0008, "SIMD stays portable"). The worktree and its build are removed on exit; the renders stay in `build/null/current` and `build/null/ref`.

The render scenes (`tests/render/RenderScenes.h`) are data the tool and the comparison share, so a scene added there is rendered and compared on both sides. Each is stereo, one second of full-scale noise bursts (100 ms loud, 100 ms at −34 dB, seeded, so every run renders the same input) through 97-sample blocks, not a multiple of the Engine's 16-sample run, at 44.1, 96 and 192 kHz: one Band of every Shape, Low Cut and High Cut with Brickwall too; a Bell in every Stereo Placement; a Dynamic Bell with the Band Detection Range and Auto Threshold, with the Free Detection Range and a manual Threshold, with the Sidechain as Detection Source (fed bursts offset from the main input's) and with the Sidechain as Detection Source but none connected, from the start and switched to mid-movement; a Metered Band moving and one idle (Dynamic Range 0), only metered; 24 Bells and the CPU budget's 24 Dynamic Bells; a glide of Frequency and Gain, a crossfade from a Bell to a Low Cut, Solo, Detection Audition and Global Bypass switched on a quarter of a second in; Output Gain with Pan and Phase Invert, Pan in Mid/Side, Auto Gain and Gain Scale; and the Output gliding: Output Gain, Pan and Phase Invert changed, Pan in Mid/Side changed, and Auto Gain switched on, a quarter of a second in. `tests/engine/RenderScenesTest.cpp` keeps the list covering all of this and every render repeatable.

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

**Static layers.** What doesn't change from frame to frame is drawn once into an image at the device scale it is drawn at and drawn back from it pixel for pixel (`staple::CachedLayer`, `staple::SoftShadowCache`): the window's background, drawn again on a new window size or scale, so a display or Output Meter repaint no longer renders its three full-window gradients; each grid and Analyzer scale label, drawn again when it changes; and each handle's soft shadow, per size, scale and place within a device pixel. `staple::font` makes each weight and size once. Two things stay drawn directly, measured: the grid's lines, a few dozen rectangles that cost less than drawing a display-sized image over them, and the selected handle's glow, whose faint layers of colour round off by 3 levels from an image, past the screens' 2. An image is drawn back pixel for pixel only where the display starts on a whole device pixel (`staple::startsOnWholeDevicePixel`): always at 100% and 200%, and at 150% on a 2× screen. At 75% and 125% on a 1× screen it starts half a pixel in, where an image would come back resampled and softer, so there the labels and shadows are drawn directly, as before.

| Machine | Display paint (idle frame) | Busy frame |
| --- | --- | --- |
| Apple M3, load average 46 to 61 from parallel builds, before the static layers (`fa94139`) | 69 to 79 ms | 333 to 338 ms |
| Apple M3, the same load, with them (`8e3d290`) | 72 to 73 ms | 169 to 193 ms |

Measured 2026-10-12, three runs each, alternating. The busy frame, the whole editor with its background, about halves; the idle frame, the display alone, shows no clear change at that load. Drawn 200 times at 2× (load about 100), the labels went from 10 to 33 ms to 0.2 to 1.0 ms a paint, the 24 handles' shadows from 7.7 to 13 ms to 0.5 ms. Quiet-machine figures are still to be taken.

## Footprint

`scripts/check.sh size` measures what ships. It builds the shipping configuration (Release, `EQ1_LTO=ON`, stripped as `EQ1_STRIP` leaves Release by default, Universal) in `build-size` and prints four reports; local only, CI doesn't run it. On Windows it prints the Engine heap only.

**The shipping build.** Every format's binary is linked with `-dead_strip` (`/OPT:REF /OPT:ICF` on Windows), and the JUCE codecs eq1 never uses (FLAC, Ogg Vorbis, Opus, MP3, Windows Media, WebP) are compiled out (`plugin/CMakeLists.txt`). `EQ1_STRIP` decides stripping per configuration. Left unset, every configuration but Debug strips and is dead-stripped, and a Debug build keeps its symbols and unused code for the debugger (Xcode and Visual Studio decide each configuration separately). `ON` strips and dead-strips every configuration. `OFF` strips none but dead-strips as by default; `scripts/check.sh` configures its Release test build that way, like `EQ1_LTO=OFF`. A stripping build compiles with debug information, writes it to a `.dSYM` beside each bundle (`eq1.vst3.dSYM` next to `eq1.vst3`, about 140 MB each), strips the binary of its local symbols and signs it ad hoc again. A crash log is symbolicated with `atos -o eq1.vst3.dSYM/Contents/Resources/DWARF/eq1 -arch arm64 -l <load address> <address>`; `dwarfdump --uuid` on the binary and the `.dSYM` agree. The report's "Debug symbols" step checks that for every format and architecture, and that the entry function resolves to a source line; it fails otherwise. Archiving the `.dSYM`s and the release signature are #16's.

**Binary size.** Each format's binary per architecture, before (`perf-integration` `cbff7b7`: no dead-stripping, every codec in, unstripped) and as it ships now, and the universal file. Apple clang 17, 2026-10-12:

| Format | arm64 before | arm64 now | x86_64 before | x86_64 now | Universal now |
| --- | --- | --- | --- | --- | --- |
| VST3 | 8,394,976 | 5,634,000 (−32.9%) | 9,125,424 | 6,110,048 (−33.0%) | 11,761,616 |
| AU | 8,413,264 | 5,652,496 (−32.8%) | 9,031,848 | 6,136,416 (−32.1%) | 11,812,880 |
| AAX | 8,421,360 | 5,635,456 (−33.1%) | 9,029,000 | 6,108,368 (−32.3%) | 11,763,072 |
| CLAP | 8,416,672 | 5,602,720 (−33.4%) | 9,020,296 | 6,063,392 (−32.8%) | 11,697,568 |
| Standalone | 8,300,544 | 5,967,120 (−28.1%) | 8,881,408 | 6,436,336 (−27.5%) | 12,422,416 |

Of the arm64 VST3's 2.76 MB saved, `strip -x` alone would have taken 1.71 MB (the "stripped" column `check.sh size` printed before this build stripped itself); the codecs and dead code are the other 1.05 MB, 0.91 MB of it `__text`.

**Code by library.** The VST3's `__text` attributed by `scripts/code-by-library.py`: each symbol of the `.dSYM`'s symbol table (the stripped binary keeps its exports only) is looked up in the normal build's objects (the LTO link's own map attributes everything to one `lto.o`), and one no object or several define (template instances, LTO-renamed functions) goes by the namespace in its name. Same build; the arm64 `__text` is 4,180,180 bytes, the x86_64 one 4,616,160:

| Library | arm64 | x86_64 |
| --- | --- | --- |
| juce_gui_basics | 893,092 | 977,840 |
| eq1 Plugin Shell | 879,864 | 938,016 |
| juce_graphics | 850,208 | 986,064 |
| harfbuzz | 668,692 | 744,496 |
| juce_core | 198,088 | 214,624 |
| juce_audio_plugin_client | 107,984 | 117,456 |
| lunasvg | 98,424 | 114,464 |
| codec: libjpg | 94,196 | 108,256 |
| static initialisers | 79,408 | 62,768 |
| eq1 Engine | 46,276 | 66,240 |
| juce_data_structures | 35,848 | 36,400 |
| JUCE templates (by namespace) | 35,844 | 39,040 |
| juce_audio_processors_headless | 35,460 | 45,264 |
| juce_audio_processors | 28,836 | 27,552 |
| codec: zlib | 27,172 | 30,720 |
| codec: libpng | 24,892 | 28,688 |
| juce_events | 17,464 | 18,112 |
| libc++ templates | 16,724 | 17,888 |
| juce_audio_formats | 12,260 | 14,000 |
| juce_dsp | 9,656 | 9,504 |
| juce_audio_basics | 9,236 | 10,496 |
| other | 7,424 | 5,792 |
| juce_gui_extra | 2,084 | 1,456 |
| sheenbidi | 732 | 736 |

What remains of code eq1 never runs: libjpg (94 KB; JUCE has no flag for it, `JUCE_INCLUDE_JPEGLIB_CODE=0` wants an external libjpeg instead), and lunasvg (98 KB), JUCE's SVG parser. libpng and zlib stay on purpose: the screens tests write PNGs. harfbuzz (0.67 MB) shapes every string the editor draws.

**Engine heap.** `tests/engine/FootprintTest.cpp` (tagged `[footprint]`, in the normal test run) constructs one `Engine`, prepares it stereo, and reads from `eq1::test::AllocationGuard` what the heap holds: the bytes the allocator gave out, less what was freed meanwhile, in the allocator's own block sizes (`malloc_size`, `_msize`). It fails above its ceilings, `heapCeilingBytes48k` and `heapCeilingBytes192k`, set about 5% above the figures measured here; a change that shrinks the Engine lowers them.

| Sample rate | Heap per stereo Engine | Ceiling |
| --- | --- | --- |
| 48 kHz | 443,776 bytes | 466,000 |
| 192 kHz | 787,840 bytes | 827,000 |

Measured 2026-10-12 on an Apple M3 (`177-buffers-sized-to-need`). Most of it is the Bands' and detectors' state. The Analyzer's three taps and the Output Level's two channels hold **114,880 bytes** together at 48 kHz stereo, against a ceiling of 200,000 in the same test (`tapsCeilingBytes48k`), which `scripts/check.sh size` prints beside the heap:

- An Analyzer tap (`engine/src/AnalysisFifo.h`) is a ring of the smallest power of two covering 150 ms at the prepared sample rate: 8,192 samples at 44.1 and 48 kHz, 16,384 at 96 kHz, 32,768 at 192 kHz, indexed with a mask. A ring is never freed or moved while the Engine lives (a prepare that needs a bigger one adds it and keeps the old one), so the editor reads through any re-prepare. `Engine::analysisCapacity()` tells the editor how big its read buffer is. A full ring drops new samples, so while no editor reads, a tap fills and keeps up to 150 ms of old audio; the opening editor skips it (`Engine::skipAnalysis()`, which moves the reader's index up to the writer's), so its first Analyzer frame shows only audio from after it opened.
- **Outgrown rings are kept.** The editor may be popping from a ring while the host prepares the Engine again, on another thread, and nothing tells the reader when it is done with a ring, so freeing a replaced ring would be a use-after-free. Each tap keeps every ring it has outgrown until the Engine is destroyed, and a prepare that needs less keeps the bigger ring it has. The cost is bounded by the rates eq1 supports (44.1 to 192 kHz), whose rings are 8,192, 16,384 and 32,768 floats: at worst a tap has made all three, 57,344 floats, where 8,192 would do at 48 kHz. Over the three taps that is **589,824 bytes** more than a fresh 48 kHz Engine holds (49,152 floats × 4 bytes × 3); a single 192 kHz prepare and back to 48 kHz leaves 393,216 bytes more (the 32,768 ring in use and the 8,192 one kept). The heap figures above are for a freshly prepared Engine, which has no outgrown rings.
- An Output Level tap (`engine/src/OutputLevelTap.h`) keeps its 300 ms RMS window as per-run (16-sample) sums of squares in double: 900 doubles at 48 kHz. The window is rounded up to whole 16-sample runs and the RMS is the mean over the rounded window, so it is 300 ms exactly at 48, 96 and 192 kHz and up to 8 samples longer at 44.1, 88.2 and 176.4 kHz (13,232 samples rather than 13,230 at 44.1 kHz, 0.05 ms more). A second sum, started afresh each window, replaces the running one once it has covered a whole window, so rounding never builds up and the audio thread never resums the window. The two taps are allocated once, for stereo, and the live channel count is atomic, so a mono/stereo re-prepare frees nothing under the Output Meter's reads.

## Subnormal numbers

`Engine::process()` flushes subnormal numbers to zero for its own duration (`engine/src/NoSubnormals.h`), whatever the host's floating-point mode, and restores that mode afterwards. Without it, a filter ringing down after the input stops outputs subnormal floats for seconds, and its double-precision state passes through the subnormal range after that.

## Identity sections

A Band's cascade keeps a fixed number of biquads per structure, so glides can interpolate; some of them are exactly the identity: an unsplit resonant shelf's second biquad (`docs/dsp/filter-design.md`, "Resonant shelves"), whose matched poles are cancelled by equal zeros (`b0 = 1`, `b1 = a1`, `b2 = a2`, `engine/src/Biquad.h`). A section that is exactly the identity at both ends of a run, so at every interpolated position in it, and whose state is clear on every line it runs on, is skipped for the piece, and the section count doesn't change. One whose state still holds what it filtered before it became the identity runs until that has drained: cancelled poles ring down at their own rate. Where nothing fuses (x64, MSVC), running such a section from a clear state leaves its input and state exactly as they are, so skipping it is bit-identical. On arm64, where clang fuses the step's multiply-adds (ADR 0008, "SIMD stays portable"), `a1 * x - a1 * y` fused leaves the rounding residual of one product (about 1e-16 of the signal) in the state, which the cancelled poles carry: running the section adds that residual to its output, where it shows in a float sample only on input samples very close to zero (a sine's zero crossings), and skipping it leaves it out. So on arm64 skipping is exact on every render scene but not on every signal: it differs from running, at most, by those residuals, near −300 dBFS. For the same reason a section that has run on arm64 drains to a clear state only in digital silence. A Bell at 0 dB is designed as a matched section whose numerator equals its denominator in the analog domain, which the three-point match reproduces only to about 1e-14, so it is not an exact identity and is not skipped. `Engine::sectionsSkipped()` counts the skipped passes, for tests (`tests/engine/IdentitySectionsTest.cpp`).

## Non-finite input

A NaN or infinite input sample, from the host or an upstream plugin, never reaches the output (`docs/adr/0008-performance-never-costs-sound.md`, "Non-finite input"): `Engine::process()` replaces any non-finite output sample with 0, after the output stage and Global Bypass, and the Analyzer taps push 0 in its place. Every Band's filter chain (a detector's Detection Range filters and the Solo region included) checks its state once per run, at the run's start, and clears any section that isn't finite, starting afresh at that run's design with no glide from the filter it lost; a detector that heard one starts afresh at the run's end, Auto Threshold included, and its Band rests at its Gain over the next run. So a Band recovers within one 16-sample run, and from the next run on the output is what a freshly prepared Engine plays on the same material (`tests/engine/NonFiniteInputTest.cpp`). On finite input the output is bit-identical to before the checks, which cost one comparison per section per run, six per detector per run and one per output sample.

## Sample rates and block sizes

eq1 is checked at 44.1, 48, 88.2, 96, 176.4 and 192 kHz, and at block sizes from 1 sample up, changing from call to call. The output is the same, sample for sample, however the host cuts the audio into blocks, Dynamic Bands included. The Engine runs on a grid of 16-sample runs that starts at `prepare()` and carries across `process()` calls: a block ending mid-run leaves the run open for the next one. Everything timed per run (a Band's re-design and glides, Auto Attack, Auto Threshold) happens once per grid run, and a Dynamic Band's filter over a run follows only what its detector heard in earlier runs, so it reacts up to one run (16 samples, about 0.33 ms at 48 kHz) after the sound, with no added latency. Settings still arrive per block.
