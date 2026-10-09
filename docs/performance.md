# Performance and robustness

## CPU budget

`tests/cpu/CpuBudget.cpp` (`scripts/check.sh cpu`, its own CI step) measures the Engine with 24 Dynamic Bands: Bells across the spectrum, stereo, 512-sample blocks, on noise that is loud for 100 ms of every 200 so every Band keeps moving and redesigning its filter. The load is the median of five 4-second runs, in percent of real time.

| Sample rate | Ceiling | Apple M3 (2026-10-09) |
| --- | --- | --- |
| 48 kHz | 12% | 3.7% |
| 96 kHz | 20% | 7.4% |

The ceilings are set from the M3's load, with room for slower CI runners, until the runners' own numbers are recorded here; they still fail on a regression that multiplies the cost. They are absolute, not relative to a baseline, so runner noise doesn't fail the check.

The same run fails if silence (after every filter has rung down) or input made of subnormal numbers costs more than 1.5 times the music. That catches arithmetic on subnormal numbers, many times slower than normal on x64; on arm64 it runs at full speed, so the Windows x64 run is the one that would catch it.

## Subnormal numbers

`Engine::process()` flushes subnormal numbers to zero for its own duration (`engine/src/NoSubnormals.h`), whatever the host's floating-point mode, and restores that mode afterwards. Without it, a filter ringing down after the input stops outputs subnormal floats for seconds, and its double-precision state passes through the subnormal range after that.

## Sample rates and block sizes

eq1 is checked at 44.1, 48, 88.2, 96, 176.4 and 192 kHz, and at block sizes from 1 sample up, changing from call to call. Without Dynamic Bands the output is the same, sample for sample, however the host cuts the audio into blocks. A Dynamic Band's gain moves once per run of at most 16 samples, and a run also ends where a host block ends, so with Dynamic Bands the output differs slightly with block size: at worst about 45 dB below the peak, with 1-sample blocks. Matching to the sample would need a run to use only detection from earlier runs, a change to how dynamics are timed.
