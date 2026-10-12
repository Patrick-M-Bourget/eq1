# Performance and robustness

## CPU budget

`tests/cpu/CpuBudget.cpp` (`scripts/check.sh cpu`, its own CI step) measures the Engine with 24 Dynamic Bands: Bells across the spectrum, stereo, 512-sample blocks, on noise that is loud for 100 ms of every 200 so every Band keeps moving and redesigning its filter. The load is the median of five 4-second runs, in percent of real time.

| Sample rate | Ceiling | CI Windows x64 | CI macOS (arm64) | Apple M3 |
| --- | --- | --- | --- | --- |
| 48 kHz | 35% | 9.6–11.0% | 5.9% | 3.7% |
| 96 kHz | 60% | 18.1–19.1% | 14.6% | 7.4% |

Measured 2026-10-09; the CI columns are GitHub's `windows-latest` and `macos-latest` runners. The ceilings are about three times the slowest runner's worst run: high enough that runner noise doesn't fail the check, low enough to fail on a regression that multiplies the cost. They are absolute, not relative to a baseline.

The same run fails if silence (after every filter has rung down) or input made of subnormal numbers costs more than 1.5 times the music. That catches arithmetic on subnormal numbers, many times slower than normal on x64; on arm64 it runs at full speed, so the Windows x64 run is the one that would catch it.

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

## Subnormal numbers

`Engine::process()` flushes subnormal numbers to zero for its own duration (`engine/src/NoSubnormals.h`), whatever the host's floating-point mode, and restores that mode afterwards. Without it, a filter ringing down after the input stops outputs subnormal floats for seconds, and its double-precision state passes through the subnormal range after that.

## Sample rates and block sizes

eq1 is checked at 44.1, 48, 88.2, 96, 176.4 and 192 kHz, and at block sizes from 1 sample up, changing from call to call. The output is the same, sample for sample, however the host cuts the audio into blocks, Dynamic Bands included. The Engine runs on a grid of 16-sample runs that starts at `prepare()` and carries across `process()` calls: a block ending mid-run leaves the run open for the next one. Everything timed per run (a Band's re-design and glides, Auto Attack, Auto Threshold) happens once per grid run, and a Dynamic Band's filter over a run follows only what its detector heard in earlier runs, so it reacts up to one run (16 samples, about 0.33 ms at 48 kHz) after the sound, with no added latency. Settings still arrive per block.
