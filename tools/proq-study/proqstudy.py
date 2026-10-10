#!/usr/bin/env python3
"""Pro-Q study: turn the installed Pro-Q 4 factory presets into a private corpus and per-category stats in eq1's terms.

For Preset authors to read (spec #1, "Factory Presets"). Pro-Q 4's presets are a private reference: everything this
tool writes goes under the git-ignored build directory, and nothing derived from them is committed. Standard
library only.

    python3 tools/proq-study/proqstudy.py corpus [--source <dir>] [--out <dir>]
    python3 tools/proq-study/proqstudy.py stats [--source <dir>] [--out <dir>]
    python3 -m unittest discover -s tools/proq-study

The source defaults to ~/Documents/FabFilter/Presets/Pro-Q 4, the output to build/proq-study in this checkout:
corpus.json and corpus.txt (every preset's Bands in use, in eq1's terms) and stats.md (per category).

Calibration: the mappings under "Mappings" are derived from the stored values alone. Frequency (log2 of Hz),
Shape (ADR 0003) and Gain (dB) are certain. Slope's points and how fractions interpolate, Stereo Placement's
order, Q's range and Threshold's range are not yet confirmed against Pro-Q 4's own display: each needs a few
presets checked in a host (#135), and any correction goes here.

Anything eq1 leaves out is flagged, never dropped: spectral dynamics, Character, a Processing Mode other than Zero
Latency, surround Speakers, and Brickwall on a Shape other than a Cut. Keys the tool doesn't know are ignored.
"""

import argparse
import collections
import json
import math
import statistics
import sys
from pathlib import Path

DEFAULT_SOURCE = Path.home() / "Documents" / "FabFilter" / "Presets" / "Pro-Q 4"
DEFAULT_OUT = Path(__file__).resolve().parents[2] / "build" / "proq-study"


# --- Mappings ---------------------------------------------------------------------------------------------------
#
# Derived from the presets' own values; see "Calibration" at the top for which are confirmed in Pro-Q 4's UI.

# Shape index -> eq1 Shape; Pro-Q 4's order is eq1's (ADR 0003).
SHAPES = ["Bell", "Low Shelf", "Low Cut", "High Shelf", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt",
          "All Pass"]

# Slope is stored continuously as a position on Pro-Q's Slope menu. Its whole points are these dB/oct, then
# Brickwall at 9. A fraction lies on a straight line in dB/oct between the two points around it (2.5 is 21 dB/oct,
# 6.5 is 60). Above 8 short of 9 it stays at 96.
SLOPE_POINTS = [6.0, 12.0, 18.0, 24.0, 30.0, 36.0, 48.0, 72.0, 96.0]
BRICKWALL_POINT = 9

# Stereo Placement index -> eq1 Stereo Placement. 2, the default, is Stereo; presets named for Mid or Side use 3 and
# 4; the "ST" and "Unlinked Stereo" presets pair 0 and 1 Bands, taken as Left then Right.
PLACEMENTS = ["Left", "Right", "Stereo", "Mid", "Side"]

# Q is stored normalised, log-even over Pro-Q's (and eq1's) 0.025 to 40: the default 0.5 is Q 1.
Q_MIN, Q_MAX = 0.025, 40.0


# Threshold is stored normalised. Its top, 1, is Auto Threshold: nearly every Band in the factory presets sits there,
# with or without Pro-Q's "Dynamics Auto" switch, which eq1 has no counterpart for and the corpus keeps as is.
# Below the top it is taken as linear over eq1's -60 to 0 dB (Pro-Q's default, 2/3, is -20 dB).
THRESHOLD_MIN_DB, THRESHOLD_MAX_DB = -60.0, 0.0

# Shapes with a Gain, so with dynamics (engine/include/eq1/Settings.h, hasGain).
GAIN_SHAPES = {"Bell", "Low Shelf", "High Shelf", "Tilt Shelf", "Flat Tilt"}
CUT_SHAPES = {"Low Cut", "High Cut"}

# What eq1 leaves out, flagged without dropping the Band or preset.
PROCESSING_MODES = ["Zero Latency", "Natural Phase", "Linear Phase"]
CHARACTERS = ["Clean", "Subtle", "Warm"]


def slope(position):
    """dB/oct for a stored Slope position, or "Brickwall"."""
    if position >= BRICKWALL_POINT:
        return "Brickwall"
    position = max(0.0, position)
    below = min(int(position), len(SLOPE_POINTS) - 1)
    if below == len(SLOPE_POINTS) - 1:
        return SLOPE_POINTS[-1]
    fraction = position - below
    return SLOPE_POINTS[below] + fraction * (SLOPE_POINTS[below + 1] - SLOPE_POINTS[below])


def choice(names, index):
    index = int(round(index))
    return names[index] if 0 <= index < len(names) else f"unknown ({index})"


def parse_ffp(text):
    """The key=value lines of an .ffp file, outside its section headers."""
    values = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("[") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def number(values, key, default=0.0):
    try:
        return float(values[key])
    except (KeyError, ValueError):
        return default


def read_band(values, slot):
    def raw(name, default=0.0):
        return number(values, f"Band {slot} {name}", default)

    shape = choice(SHAPES, raw("Shape"))
    stored_slope = slope(raw("Slope", 2.0))
    threshold = raw("Threshold", 2.0 / 3.0)
    flags = []
    if raw("Spectral Enabled") == 1:
        flags.append("spectral dynamics")
    if raw("Speakers", 1.0) != 1:
        flags.append("surround Speakers")
    if stored_slope == "Brickwall" and shape not in CUT_SHAPES:
        flags.append(f"Brickwall on {shape}")
    return {
        "slot": slot,
        "shape": shape,
        "frequency": 2.0 ** raw("Frequency", math.log2(1000.0)),
        "gain": raw("Gain"),
        "q": Q_MIN * (Q_MAX / Q_MIN) ** raw("Q", 0.5),
        "slope": stored_slope,
        "placement": choice(PLACEMENTS, raw("Stereo Placement", 2.0)),
        "bypass": raw("Enabled", 1.0) == 0,
        "dynamic": shape in GAIN_SHAPES and raw("Dynamic Range") != 0,
        "dynamicRange": raw("Dynamic Range"),
        "thresholdAuto": threshold >= 1.0,
        "threshold": THRESHOLD_MIN_DB + threshold * (THRESHOLD_MAX_DB - THRESHOLD_MIN_DB),
        "attack": raw("Attack", 50.0),
        "release": raw("Release", 50.0),
        "dynamicsBypass": raw("Dynamics Enabled", 1.0) == 0,
        "detectionSource": "External" if raw("External Side Chain") == 1 else "Internal",
        "detectionRange": "Free" if raw("Side Chain Filtering") == 1 else "Band",
        "detectionLow": 2.0 ** raw("Side Chain Low Frequency", math.log2(20.0)),
        "detectionHigh": 2.0 ** raw("Side Chain High Frequency", math.log2(20000.0)),
        "proqDynamicsAuto": raw("Dynamics Auto", 1.0) == 1,
        "flags": flags,
    }


def read_preset(path, source):
    path = Path(path)
    values = parse_ffp(path.read_text(encoding="utf-8", errors="replace"))
    relative = path.relative_to(source)
    bands = [read_band(values, slot) for slot in range(1, 25) if number(values, f"Band {slot} Used") == 1]
    flags = []
    mode = choice(PROCESSING_MODES, number(values, "Processing Mode"))
    if mode != "Zero Latency":
        flags.append(f"Processing Mode {mode}")
    character = choice(CHARACTERS, number(values, "Character"))
    if character != "Clean":
        flags.append(f"Character {character}")
    return {
        "category": relative.parts[0] if len(relative.parts) > 1 else "(top level)",
        "name": path.stem,
        "bands": bands,
        "flags": flags,
    }


def is_flagged(preset):
    return bool(preset["flags"]) or any(band["flags"] for band in preset["bands"])


def read_presets(source):
    return [read_preset(path, source) for path in sorted(Path(source).rglob("*.ffp"))]


# --- Corpus -------------------------------------------------------------------------------------------------------

def hz(frequency):
    return f"{frequency / 1000:.3g} kHz" if float(f"{frequency:.3g}") >= 1000 else f"{frequency:.3g} Hz"


def describe_band(band):
    parts = [f"{band['slot']:>2} {band['shape']}", hz(band["frequency"])]
    if band["shape"] in GAIN_SHAPES:
        parts.append(f"Gain {band['gain']:+.1f} dB")
    parts.append(f"Q {band['q']:.3g}")
    if band["shape"] not in ("Bell", "Flat Tilt"):
        parts.append("Brickwall" if band["slope"] == "Brickwall" else f"Slope {band['slope']:.3g} dB/oct")
    if band["placement"] != "Stereo":
        parts.append(band["placement"])
    if band["bypass"]:
        parts.append("Bypassed")
    if band["dynamic"]:
        threshold = "Auto" if band["thresholdAuto"] else f"{band['threshold']:.1f} dB"
        dynamics = f"Dynamic Range {band['dynamicRange']:+.1f} dB, Threshold {threshold}, " \
                   f"Attack {band['attack']:.0f}%, Release {band['release']:.0f}%"
        if band["detectionRange"] == "Free":
            dynamics += f", Free {hz(band['detectionLow'])} to {hz(band['detectionHigh'])}"
        if band["detectionSource"] == "External":
            dynamics += ", External"
        if band["dynamicsBypass"]:
            dynamics += ", Dynamics Bypass"
        parts.append(f"[{dynamics}]")
    if band["flags"]:
        parts.append("FLAGGED: " + ", ".join(band["flags"]))
    return ", ".join(parts)


def corpus_text(presets):
    lines = []
    for preset in presets:
        lines.append(f"{preset['category']} / {preset['name']}")
        if preset["flags"]:
            lines.append("   FLAGGED: " + ", ".join(preset["flags"]))
        lines.extend("  " + describe_band(band) for band in preset["bands"])
        lines.append("")
    return "\n".join(lines)


def write_corpus(presets, out):
    (out / "corpus.json").write_text(json.dumps(presets, indent=1))
    (out / "corpus.txt").write_text(corpus_text(presets))
    flagged = sum(1 for preset in presets if is_flagged(preset))
    return f"{len(presets)} presets, {flagged} with flagged features: {out / 'corpus.txt'}"


# --- Stats --------------------------------------------------------------------------------------------------------

def octave_clusters(frequencies):
    """How many frequencies fall in each octave band, labelled by its centre (1 kHz times a power of 2)."""
    bands = collections.Counter(round(math.log2(frequency / 1000.0)) for frequency in frequencies)
    return [(1000.0 * 2.0 ** octave, bands[octave]) for octave in sorted(bands)]


def spread(values, format_value):
    values = sorted(values)
    if values[0] == values[-1]:
        return format_value(values[0])
    return f"{format_value(values[0])} to {format_value(values[-1])} (median {format_value(statistics.median(values))})"


def counts(counter, order=None):
    keys = order if order is not None else [key for key, _ in counter.most_common()]
    return ", ".join(f"{key} {counter[key]}" for key in keys if counter[key])


def category_stats(title, presets):
    bands = [band for preset in presets for band in preset["bands"]]
    lines = [f"## {title} ({len(presets)} presets)", ""]
    sizes = collections.Counter(len(preset["bands"]) for preset in presets)
    lines.append("- Bands per preset: " + ", ".join(f"{size} x{sizes[size]}" for size in sorted(sizes)))
    shapes = collections.Counter(band["shape"] for band in bands)
    lines.append("- Shapes: " + counts(shapes))
    lines.append("- Stereo Placement: " + counts(collections.Counter(band["placement"] for band in bands),
                                                 ["Stereo", "Left", "Right", "Mid", "Side"]))
    dynamic = [band for band in bands if band["dynamic"]]
    in_presets = sum(1 for preset in presets if any(band["dynamic"] for band in preset["bands"]))
    cut = sum(1 for band in dynamic if band["dynamicRange"] < 0)
    line = f"- Dynamic Bands: {len(dynamic)} of {len(bands)} Bands, in {in_presets} of {len(presets)} presets; " \
           f"{cut} cut, {len(dynamic) - cut} boost"
    if dynamic:
        line += f"; Dynamic Range {spread([band['dynamicRange'] for band in dynamic], lambda v: f'{v:+.1f} dB')}"
        line += f"; Auto Threshold {sum(1 for band in dynamic if band['thresholdAuto'])}"
        line += f", Free Detection Range {sum(1 for band in dynamic if band['detectionRange'] == 'Free')}"
        line += f", External {sum(1 for band in dynamic if band['detectionSource'] == 'External')}"
    lines.append(line)
    lines.append(f"- Presets with flagged features: {sum(1 for preset in presets if is_flagged(preset))}")
    lines.append("")
    for shape, _ in shapes.most_common():
        of_shape = [band for band in bands if band["shape"] == shape]
        lines.append(f"### {shape} ({len(of_shape)})")
        lines.append("")
        lines.append("- Frequency by octave: " + ", ".join(
            f"{hz(centre)} {count}" for centre, count in octave_clusters([band["frequency"] for band in of_shape])))
        lines.append(f"- Frequency: {spread([band['frequency'] for band in of_shape], hz)}")
        if shape in GAIN_SHAPES:
            lines.append(f"- Gain: {spread([band['gain'] for band in of_shape], lambda v: f'{v:+.1f} dB')}")
        lines.append(f"- Q: {spread([band['q'] for band in of_shape], lambda v: f'{v:.3g}')}")
        if shape not in ("Bell", "Flat Tilt"):
            slopes = collections.Counter(
                band["slope"] if band["slope"] == "Brickwall" else f"{band['slope']:.3g}" for band in of_shape)
            lines.append("- Slope (dB/oct): " + counts(slopes))
        lines.append("")
    return lines


def stats_report(presets):
    lines = ["# Pro-Q 4 factory presets in eq1's terms", "",
             "Private study for Preset authors; generated by tools/proq-study/proqstudy.py. Not for committing.", ""]
    lines += category_stats("All categories", presets)
    by_category = collections.defaultdict(list)
    for preset in presets:
        by_category[preset["category"]].append(preset)
    for category in sorted(by_category):
        lines += category_stats(category, by_category[category])
    return "\n".join(lines)


def write_stats(presets, out):
    (out / "stats.md").write_text(stats_report(presets))
    return f"{len(presets)} presets: {out / 'stats.md'}"


# --- Command line -------------------------------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=["corpus", "stats"])
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="Pro-Q 4's preset folder")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="where to write (keep it git-ignored)")
    args = parser.parse_args(argv)
    if not args.source.is_dir():
        print(f"proqstudy: no Pro-Q 4 preset folder at {args.source} (give one with --source)", file=sys.stderr)
        return 1
    presets = read_presets(args.source)
    if not presets:
        print(f"proqstudy: no .ffp presets under {args.source}", file=sys.stderr)
        return 1
    args.out.mkdir(parents=True, exist_ok=True)
    print((write_corpus if args.command == "corpus" else write_stats)(presets, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
