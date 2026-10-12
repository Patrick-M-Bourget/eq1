# Performance and robustness

## CPU budget

`tests/cpu/CpuBudget.cpp` (`scripts/check.sh cpu`, its own CI step) measures the Engine with 24 Dynamic Bands: Bells across the spectrum, stereo, 512-sample blocks, on noise that is loud for 100 ms of every 200 so every Band keeps moving and redesigning its filter. The load is the median of five 4-second runs, in percent of real time.

| Sample rate | Ceiling | CI Windows x64 | CI macOS (arm64) | Apple M3 |
| --- | --- | --- | --- | --- |
| 48 kHz | 35% | 9.6–11.0% | 5.9% | 3.7% |
| 96 kHz | 60% | 18.1–19.1% | 14.6% | 7.4% |

Measured 2026-10-09; the CI columns are GitHub's `windows-latest` and `macos-latest` runners. The ceilings are about three times the slowest runner's worst run: high enough that runner noise doesn't fail the check, low enough to fail on a regression that multiplies the cost. They are absolute, not relative to a baseline.

The same run fails if silence (after every filter has rung down) or input made of subnormal numbers costs more than 1.5 times the music. That catches arithmetic on subnormal numbers, many times slower than normal on x64; on arm64 it runs at full speed, so the Windows x64 run is the one that would catch it.

## Paint time

`tests/plugin/PaintTimeTest.cpp` (`scripts/check.sh paint`) measures how long the whole editor takes to draw a busy frame: 1200 × 760 rendered into an image at 2× (`createComponentSnapshot` with scale 2), with 24 Dynamic Bells across the spectrum, one selected, the Analyzer showing Pre, Post, Sidechain and Peak Hold on spectra fed with noise on the main input and the Sidechain, and the Output Meter shown. It prints the median of 20 frames and fails when that median is above its ceiling of 200 ms, about 3× the median measured with the Staple display (the grid, the Analyzer, every Band's curve with its fill, the sum's halo and the edge fades). Every later change to the editor's drawing stays under it.

The test is tagged `[.paint]`, hidden from the normal run so its timing never flakes it. `scripts/check.sh paint` builds and runs it, and is local only: CI doesn't run it. Like the CPU budget, it measures nothing on a Mac busy with other work (a load average above half the cores) and exits 3, which `scripts/check.sh` with no stage skips. To time it anyway, run `build/tests/eq1_plugin_tests "[paint]"`.

| Machine (quiet) | Median frame |
| --- | --- |
| Apple M3, before the Staple display (`ef8a283`) | 49.1 ms |
| Apple M3, with the Staple display (`c601e21`) | 64.2 ms |

Measured 2026-10-10. On a machine busy with other builds the median reached 125 to 160 ms at a load average above 80: the ceiling allows for some load, but not that much.

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

**Engine heap.** `tests/engine/FootprintTest.cpp` (tagged `[footprint]`, in the normal test run) constructs one `Engine`, prepares it stereo, and reads from `eq1::test::AllocationGuard` what the heap holds: the bytes the allocator gave out, less what was freed meanwhile, in the allocator's own block sizes (`malloc_size`, `_msize`). It fails above its ceilings, `heapCeilingBytes48k` and `heapCeilingBytes192k`, set about 5% above the figures measured here; a change that shrinks the Engine lowers them (#177).

| Sample rate | Heap per stereo Engine | Ceiling |
| --- | --- | --- |
| 48 kHz | 1,377,632 bytes | 1,450,000 |
| 192 kHz | 2,065,760 bytes | 2,170,000 |

Measured 2026-10-11 on an Apple M3 (`perf-integration`, `749fc3a`).

## Subnormal numbers

`Engine::process()` flushes subnormal numbers to zero for its own duration (`engine/src/NoSubnormals.h`), whatever the host's floating-point mode, and restores that mode afterwards. Without it, a filter ringing down after the input stops outputs subnormal floats for seconds, and its double-precision state passes through the subnormal range after that.

## Sample rates and block sizes

eq1 is checked at 44.1, 48, 88.2, 96, 176.4 and 192 kHz, and at block sizes from 1 sample up, changing from call to call. The output is the same, sample for sample, however the host cuts the audio into blocks, Dynamic Bands included. The Engine runs on a grid of 16-sample runs that starts at `prepare()` and carries across `process()` calls: a block ending mid-run leaves the run open for the next one. Everything timed per run (a Band's re-design and glides, Auto Attack, Auto Threshold) happens once per grid run, and a Dynamic Band's filter over a run follows only what its detector heard in earlier runs, so it reacts up to one run (16 samples, about 0.33 ms at 48 kHz) after the sound, with no added latency. Settings still arrive per block.
