#!/usr/bin/env python3
"""Filter lab: try a decramped filter design against its analog target before writing C++.

Mirrors the Engine's design code (engine/src/MatchedDesign.cpp, engine/src/ShapeDesign.cpp) and the
Engine tests' error measure, so a design that reports well here is worth porting. Standard library
only. See docs/dsp/filter-design.md for the targets and the design method.

    python3 tools/filter-lab/filterlab.py bell
    python3 tools/filter-lab/filterlab.py low-shelf --orders 1 2 16 --q 0.71 40
    python3 tools/filter-lab/filterlab.py high-cut --orders 1 2 16 32 --q 0.71 10
    python3 tools/filter-lab/filterlab.py band-pass --orders 1 4 16 --q 0.1 2 40
    python3 tools/filter-lab/filterlab.py all-pass --orders 1 2 8 --q 0.71
    python3 tools/filter-lab/filterlab.py gain-computer --overshoots 9 12 15
    python3 tools/filter-lab/filterlab.py auto-threshold --spreads 0.1 0.2 0.3 --falls 1 1.25 2
    python3 tools/filter-lab/filterlab.py high-cut --slopes 15 21 87 --target blend
    python3 tools/filter-lab/filterlab.py slopes
    python3 tools/filter-lab/filterlab.py slope-gaps seams shelves rescale --jobs 6

To try a new design, write a function returning (digital sections, analog target in dB) like the
ones under "Shapes", and pass it to report(). A new command registers itself with @command in its own
section, so the entry point at the bottom never changes.
"""

import argparse
import cmath
import math
from dataclasses import dataclass

PI = math.pi

# Every command line command: name -> (add its options to an argparse parser, run it on the parsed args).
COMMANDS = {}


def command(*names, options=lambda parser: None):
    """Registers the decorated run(args) as each of names, with options(parser) adding its arguments."""
    def register(run):
        for name in names:
            COMMANDS[name] = (options, run)
        return run
    return register


# --- Matched sections (engine/src/MatchedDesign.cpp) ---------------------------------------------

@dataclass
class Analog:
    """(n2 s^2 + n1 s + n0) / (d2 s^2 + d1 s + d0), s normalised to a reference frequency."""
    n2: float
    n1: float
    n0: float
    d2: float
    d1: float
    d0: float

    def squared(self, normalised_frequency):
        s = 1j * normalised_frequency
        return abs((self.n2 * s * s + self.n1 * s + self.n0) / (self.d2 * s * s + self.d1 * s + self.d0)) ** 2


@dataclass
class Biquad:
    b0: float = 1.0
    b1: float = 0.0
    b2: float = 0.0
    a1: float = 0.0
    a2: float = 0.0

    def response(self, w):
        z = cmath.exp(-1j * w)
        return (self.b0 + self.b1 * z + self.b2 * z * z) / (1 + self.a1 * z + self.a2 * z * z)


def _phi(w):
    p1 = math.sin(w / 2) ** 2
    p0 = 1 - p1
    return p0, p1, 4 * p0 * p1


def _weights(c0, c1, c2):
    return (c0 + c1 + c2) ** 2, (c0 - c1 + c2) ** 2, -4 * c0 * c2


def _factor(p0, p1, p2):
    r0, r1 = math.sqrt(max(p0, 0)), math.sqrt(max(p1, 0))
    w = 0.5 * (r0 + r1)
    c0 = 0.5 * (w + math.sqrt(max(w * w + p2, 0)))
    c1 = 0.5 * (r0 - r1)
    c2 = -p2 / (4 * c0)
    if abs(c2) > c0:
        c0, c2 = c2, c0
    return c0, c1, c2


def match_section(a: Analog, reference_hz, fs, match_hz=None):
    """Poles by matched z-transform; numerator exact at DC, at match_hz (default: the reference) and Nyquist."""
    w = min(2 * PI * reference_hz / fs, 0.98 * PI)
    nyquist = 0.5 * fs / reference_hz
    c = Biquad()
    if a.d2 == 0:
        c.a1 = -math.exp(-a.d0 / a.d1 * w)
        d0, d1, _ = _weights(1, c.a1, 0)
        r0, r1 = math.sqrt(d0 * a.squared(0)), math.sqrt(d1 * a.squared(nyquist))
        c.b0, c.b1 = 0.5 * (r0 + r1), 0.5 * (r0 - r1)
        return c
    disc = a.d1 * a.d1 - 4 * a.d2 * a.d0
    re = -a.d1 / (2 * a.d2) * w
    if disc < 0:
        im = math.sqrt(-disc) / (2 * a.d2) * w
        c.a1, c.a2 = -2 * math.exp(re) * math.cos(im), math.exp(2 * re)
    else:
        spread = math.sqrt(disc) / (2 * a.d2) * w
        c.a1, c.a2 = -(math.exp(re + spread) + math.exp(re - spread)), math.exp(2 * re)
    den = _weights(1, c.a1, c.a2)
    wm = w if match_hz is None else min(2 * PI * match_hz / fs, 0.98 * PI)
    p0, p1, p2 = _phi(wm)
    n0 = den[0] * a.squared(0)
    n1 = den[1] * a.squared(nyquist)
    at_match = den[0] * p0 + den[1] * p1 + den[2] * p2
    n2 = (at_match * a.squared(wm / (2 * PI) * fs / reference_hz) - n0 * p0 - n1 * p1) / p2
    c.b0, c.b1, c.b2 = _factor(n0, n1, n2)
    return c


def match_high_pass(a: Analog, reference_hz, fs, and_nyquist=False):
    """Poles as match_section's, zeros exactly at DC, magnitude matched at the reference (and_nyquist:
    the geometric mean of the gains matching at the reference and at Nyquist)."""
    w = min(2 * PI * reference_hz / fs, 0.98 * PI)
    c = match_section(Analog(0, 0, 1, a.d2, a.d1, a.d0), reference_hz, fs)
    c.b0, c.b1, c.b2 = (1, -2, 1) if a.d2 else (1, -1, 0)
    k = math.sqrt(a.squared(w / (2 * PI) * fs / reference_hz)) / abs(c.response(w))
    if and_nyquist:
        k = math.sqrt(k * math.sqrt(a.squared(fs / 2 / reference_hz)) / abs(c.response(PI)))
    c.b0, c.b1, c.b2 = c.b0 * k, c.b1 * k, c.b2 * k
    return c


def match_magnitudes(poles: Analog, reference_hz, fs, match_hz, at_dc, at_match, at_nyquist):
    """Poles as match_section's for poles; numerator fitting the given squared magnitudes at DC, match_hz
    and Nyquist."""
    c = match_section(Analog(0, 0, 1, poles.d2, poles.d1, poles.d0), reference_hz, fs)
    den = _weights(1, c.a1, c.a2)
    p0, p1, p2 = _phi(min(2 * PI * match_hz / fs, 0.98 * PI))
    n0, n1 = den[0] * at_dc, den[1] * at_nyquist
    n2 = ((den[0] * p0 + den[1] * p1 + den[2] * p2) * at_match - n0 * p0 - n1 * p1) / p2
    c.b0, c.b1, c.b2 = _factor(n0, n1, n2)
    return c


def match_notch(a: Analog, reference_hz, fs, zero_hz):
    """Poles as match_section's, zeros on the unit circle at zero_hz, magnitude matched at DC."""
    c = match_section(Analog(0, 0, 1, a.d2, a.d1, a.d0), reference_hz, fs)
    wz = min(2 * PI * zero_hz / fs, 0.98 * PI)
    c.b0, c.b1, c.b2 = 1, -2 * math.cos(wz), 1
    k = math.sqrt(a.squared(0)) / abs(c.response(0))
    c.b0, c.b1, c.b2 = c.b0 * k, c.b1 * k, c.b2 * k
    return c


def normalised(a: Analog):
    """The second-order section with s rescaled to its own natural frequency w0: (section, w0)."""
    w0 = math.sqrt(a.d0 / a.d2)
    return Analog(a.n2, a.n1 / w0, a.n0 / (w0 * w0), 1, a.d1 / w0, 1), w0


def inverse(c: Biquad):
    g = 1 / c.b0
    return Biquad(g, c.a1 * g, c.a2 * g, c.b1 * g, c.b2 * g)


def cascade_db(sections, f, fs):
    h = 1
    for s in sections:
        h *= s.response(2 * PI * f / fs)
    return 20 * math.log10(abs(h))


# --- Shapes (engine/src/ShapeDesign.cpp) ---------------------------------------------------------
# Each returns (digital sections, target(f) in dB).

def bell(fs, frequency, gain, q, order=None):
    a = 10 ** (abs(gain) / 40)
    boost = match_section(Analog(1, a / q, 1, 1, 1 / (a * q), 1), frequency, fs)

    def target(f):
        s = 1j * f / frequency
        aa = 10 ** (gain / 40)
        return 20 * math.log10(abs((s * s + s * aa / q + 1) / (s * s + s / (aa * q) + 1)))

    return [boost if gain >= 0 else inverse(boost)], target


def _low_shelf_sections(order, dc_gain, q):
    rz = dc_gain ** (1 / (2 * order))
    rp = 1 / rz
    pairs = order // 2
    resonance = (q / math.sqrt(0.5)) ** (1 / pairs) if pairs else 1
    for k in range(1, pairs + 1):
        sq = resonance / (2 * math.sin((2 * k - 1) * PI / (2 * order)))
        yield Analog(1, rz / sq, rz * rz, 1, rp / sq, rp * rp)
    if order % 2:
        yield Analog(0, 1, rz, 0, 1, rp)


def _analog_db(sections, f, frequency):
    return 10 * math.log10(math.prod(s.squared(f / frequency) for s in sections))


RESONANT_SHELF_Q = 2.0  # above this Q, a shelf section's extra resonance goes into a second biquad
SPLIT_IN = 2.5  # the split eases in from Q 2 to this Q
SPLIT_FULL, SPLIT_NONE = 0.8, 1.1  # zeros' natural frequency, as a share of Nyquist, where the split fades out


def _smoothstep(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def _resonant_split(a: Analog, frequency, fs, q, pairs):
    """A second-order shelf section (designed with its sharp poles at or below Frequency) as two biquads,
    [poles, zeros]: the section's poles over zeros at the Q the section has at Q 2, and the zeros' extra
    resonance, (zeros at their Q) / (zeros at that Q), designed as its inverse and inverted back. The
    split eases in from Q 2 to SPLIT_IN and fades out, geometrically in the zeros' Q, as the zeros'
    natural frequency goes from SPLIT_FULL to SPLIT_NONE x Nyquist, where the inverse's poles would be
    held; both fades are smoothsteps, so the coefficients' path has no corner. Unsplit, the second is its
    matched poles over equal zeros: exactly the identity, and the limit of the split."""
    natural = math.sqrt(a.n0 / a.n2)
    zeros_q = math.sqrt(a.n0 * a.n2) / a.n1
    x = natural * frequency / (fs / 2)
    t = math.log(SPLIT_NONE / x) / math.log(SPLIT_NONE / SPLIT_FULL)
    t = _smoothstep(t) * _smoothstep(math.log(q / RESONANT_SHELF_Q) / math.log(SPLIT_IN / RESONANT_SHELF_Q)) if q > RESONANT_SHELF_Q else 0.0
    split_q = zeros_q * (RESONANT_SHELF_Q / q) ** (t / pairs)
    resonance = match_section(Analog(1, 1 / split_q, 1, 1, 1 / zeros_q, 1), frequency * natural, fs)
    if split_q >= zeros_q:
        resonance.b0, resonance.b1, resonance.b2 = 1, resonance.a1, resonance.a2
        return [match_section(a, frequency, fs), resonance]
    poles = match_section(Analog(a.n2, math.sqrt(a.n0 * a.n2) / split_q, a.n0, a.d2, a.d1, a.d0), frequency, fs)
    return [poles, inverse(resonance)]


def _shelf_sections(sections, frequency, fs, q, order, invert):
    """Each analog section matched, second-order ones split by _resonant_split into two biquads: the one
    with the sharp zeros first, then the one with the sharp poles, so what a moving resonance stirs up
    near Nyquist isn't amplified by the other. Inverted, the two swap roles and places, so at 0 dB, where
    the two directions meet, so do the biquads in each place."""
    out = []
    for s in sections:
        if s.d2 == 0:
            out.append(inverse(match_section(s, frequency, fs)) if invert else match_section(s, frequency, fs))
            continue
        poles, zeros = _resonant_split(s, frequency, fs, q, order // 2)
        out += [inverse(poles), inverse(zeros)] if invert else [zeros, poles]
    return out


def low_shelf(fs, frequency, gain, q, order=2):
    sections = _shelf_sections(_low_shelf_sections(order, 10 ** (abs(gain) / 20), q), frequency, fs, q, order, gain < 0)
    target_sections = list(_low_shelf_sections(order, 10 ** (gain / 20), q))
    return sections, lambda f: _analog_db(target_sections, f, frequency)


def _mirrored_cut(low: Analog):
    """The High Shelf cut section for a Low Shelf boost section: mirrored (s -> 1/s) and inverted."""
    boost = (Analog(0, low.n0, low.n1, 0, low.d0, low.d1) if low.d2 == 0
             else Analog(low.n0, low.n1, low.n2, low.d0, low.d1, low.d2))
    return Analog(boost.d2, boost.d1, boost.d0, boost.n2, boost.n1, boost.n0)


def high_shelf(fs, frequency, gain, q, order=2):
    sections = _shelf_sections(map(_mirrored_cut, _low_shelf_sections(order, 10 ** (abs(gain) / 20), q)),
                               frequency, fs, q, order, gain >= 0)
    low_sections = list(_low_shelf_sections(order, 10 ** (-gain / 20), q))
    return sections, lambda f: gain + _analog_db(low_sections, f, frequency)


def tilt_shelf(fs, frequency, gain, q, order=2):
    sections, target = high_shelf(fs, frequency, gain, q, order)
    k = 10 ** (-gain / 40)
    first = sections[0]
    sections[0] = Biquad(first.b0 * k, first.b1 * k, first.b2 * k, first.a1, first.a2)
    return sections, lambda f: target(f) - gain / 2


def flat_tilt(fs, frequency, gain, q=None, order=None, lowest=5.0, highest=40000.0, count=13):
    step = math.log2(highest / lowest) / count
    k = 10 ** (gain / math.log2(1000) * step / 40)
    sections = []
    for i in range(count):
        centre = lowest * 2 ** ((i + 0.5) * step)
        zero, pole = centre / k, centre * k
        sections.append(match_section(Analog(0, pole / zero, 1, 0, 1, 1), pole, fs))
    at_frequency = cascade_db(sections, frequency, fs) if frequency < fs / 2 else 0.0
    g = 10 ** (-at_frequency / 20)
    first = sections[0]
    sections[0] = Biquad(first.b0 * g, first.b1 * g, first.b2 * g, first.a1, first.a2)
    return sections, lambda f: gain / math.log2(1000) * math.log2(f / frequency)


def _butterworth_sections(order, q):
    """Butterworth low-pass, -3 dB at s = 1, Q scaling each second-order section as for shelves."""
    pairs = order // 2
    resonance = (q / math.sqrt(0.5)) ** (1 / pairs) if pairs else 1
    for k in range(1, pairs + 1):
        sq = resonance / (2 * math.sin((2 * k - 1) * PI / (2 * order)))
        yield Analog(0, 0, 1, 1, 1 / sq, 1)
    if order % 2:
        yield Analog(0, 0, 1, 0, 1, 1)


def high_cut(fs, frequency, gain=None, q=0.71, order=2):
    """Each section matched at its damped natural frequency, held at or below half Nyquist."""
    sections = []
    for a in _butterworth_sections(order, q):
        if a.d2 == 0:
            sections.append(match_section(a, frequency, fs))
            continue
        damped = frequency * math.sqrt(max(1 - a.d1 * a.d1 / 4, 0.01))
        sections.append(match_section(a, frequency, fs, match_hz=min(damped, fs / 4)))
    target = list(_butterworth_sections(order, q))
    return sections, lambda f: _analog_db(target, f, frequency)


def low_cut(fs, frequency, gain=None, q=0.71, order=2):
    """The High Cut mirrored in frequency (s -> 1/s), zeros exactly at DC."""
    highs = [Analog(0, 1, 0, 0, 1, 1) if a.d2 == 0 else Analog(1, 0, 0, 1, a.d1, 1)
             for a in _butterworth_sections(order, q)]
    return [match_high_pass(a, frequency, fs) for a in highs], lambda f: _analog_db(highs, f, frequency)


def _reciprocal_roots(b):
    """Roots of s^2 - b s + 1."""
    d = cmath.sqrt(b * b - 4)
    return [(b + d) / 2, (b - d) / 2]


def _transformed_quadratics(order, roots_of):
    """Each upper-half-plane Butterworth low-pass pole p becomes two poles roots_of(p); each of those
    pairs with its conjugate into a real s^2 + d1 s + d0, returned as (d1, d0). The odd pole -1 is
    returned as None."""
    for a in _butterworth_sections(order, math.sqrt(0.5)):
        if a.d2 == 0:
            yield None
            continue
        p = complex(-a.d1 / 2, math.sqrt(max(1 - a.d1 * a.d1 / 4, 0)))
        yield [(-2 * x.real, abs(x) ** 2) for x in roots_of(p)]


STEEP_BAND_PASS_ORDER = 5
BAND_PASS_HOLD = 0.95  # where a steep Band Pass's upper sections are held, as a share of Nyquist


def _held_upper_section(upper: Analog, reference_hz, hold_hz, match_hz, fs):
    """Poles at hold_hz with the Q that makes the section as loud there, relative to DC, as the analog
    section is (never higher); zeros fitting a blend of the analog section's magnitude and the held
    section's own, by how far the Q was lowered."""
    analog_q = 1 / upper.d1
    q_at_hold = math.sqrt(upper.squared(hold_hz / reference_hz) / upper.squared(0))
    held_q = min(analog_q, q_at_hold)
    held = Analog(0, 0, upper.n0, 1, 1 / held_q, 1)
    blend = math.sqrt(1 - held_q / analog_q)
    squared = lambda f: upper.squared(f / reference_hz) ** (1 - blend) * held.squared(f / hold_hz) ** blend
    return match_magnitudes(held, hold_hz, fs, match_hz, squared(0), squared(match_hz), squared(fs / 2))


def band_pass(fs, frequency, gain=None, q=0.71, order=2):
    """s -> Q (s + 1/s) of a Butterworth low-pass. Each low-pass pole pair gives a lower pair, designed as
    a Low Cut section, and an upper pair, designed as a High Cut section matched at most 0.9 of its
    natural frequency; the odd pole gives one band-pass section. Above 24 dB/oct, upper sections beyond
    the hold are held there (_held_upper_section) and the whole cascade's gain is set to the target's at
    Frequency, or the hold if Frequency is above it."""
    steep = order >= STEEP_BAND_PASS_ORDER
    hold = BAND_PASS_HOLD * fs / 2
    analog, sections = [], []
    for quads in _transformed_quadratics(order, lambda p: _reciprocal_roots(p / q)):
        if quads is None:
            a = Analog(0, 1 / q, 0, 1, 1 / q, 1)
            analog.append(a)
            sections.append(match_section(a, frequency, fs))
            continue
        low, high = sorted(quads, key=lambda d: d[1])
        lower, w_low = normalised(Analog(1, 0, 0, 1, *low))
        upper, w_high = normalised(Analog(0, 0, 1 / (q * q), 1, *high))
        analog += [Analog(1, 0, 0, 1, *low), Analog(0, 0, 1 / (q * q), 1, *high)]
        sections.append(match_high_pass(lower, frequency * w_low, fs, and_nyquist=True))
        reference = frequency * w_high
        damped = math.sqrt(max(1 - upper.d1 * upper.d1 / 4, 0.01))
        match = min(reference * min(damped, 0.9), fs / 4)
        if steep and reference > hold:
            sections.append(_held_upper_section(upper, reference, hold, match, fs))
        else:
            sections.append(match_section(upper, reference, fs, match_hz=match))
    if steep:
        at = min(frequency, hold)
        k = 10 ** ((_analog_db(analog, at, frequency) - cascade_db(sections, at, fs)) / 20)
        s = sections[0]
        s.b0, s.b1, s.b2 = s.b0 * k, s.b1 * k, s.b2 * k
    return sections, lambda f: _analog_db(analog, f, frequency)


def notch(fs, frequency, gain=None, q=0.71, order=1):
    """s -> 1 / (Q (s + 1/s)) of a Butterworth low-pass of order Slope / 12: every section's zeros sit
    exactly at Frequency."""
    analog, sections = [], []
    for quads in _transformed_quadratics(order, lambda p: _reciprocal_roots(1 / (p * q))):
        for d1, d0 in quads if quads is not None else [(1 / q, 1)]:
            a = Analog(1, 0, 1, 1, d1, d0)
            b, w0 = normalised(a)
            analog.append(a)
            sections.append(match_notch(b, frequency * w0, fs, frequency))
    return sections, lambda f: _analog_db(analog, f, frequency)


def _all_pass_sections(order, q):
    for a in _butterworth_sections(order, q):
        yield Analog(1, -a.d1, 1, 1, a.d1, 1) if a.d2 else Analog(0, -1, 1, 0, 1, 1)


def all_pass(fs, frequency, gain=None, q=0.71, order=1):
    """Bilinear, prewarped at Frequency: flat magnitude can't cramp, and the phase at Frequency is exact.
    Returns (sections, analog phase(f) in radians)."""
    k = math.tan(min(PI * frequency / fs, 0.49 * PI))
    sections, analog = [], list(_all_pass_sections(order, q))
    for a in analog:
        if a.d2:
            a0 = 1 + a.d1 * k + k * k
            a1, a2 = 2 * (k * k - 1) / a0, (1 - a.d1 * k + k * k) / a0
            sections.append(Biquad(a2, a1, 1, a1, a2))
        else:
            a1 = (k - 1) / (1 + k)
            sections.append(Biquad(a1, 1, 0, a1, 0))

    def phase(f):
        s = 1j * f / frequency
        return sum(cmath.phase((a.n2 * s * s + a.n1 * s + a.n0) / (a.d2 * s * s + a.d1 * s + a.d0)) for a in analog)

    return sections, phase


SHAPES = {"bell": bell, "low-shelf": low_shelf, "high-shelf": high_shelf, "tilt-shelf": tilt_shelf,
          "flat-tilt": flat_tilt, "low-cut": low_cut, "high-cut": high_cut, "band-pass": band_pass,
          "notch": notch, "all-pass": all_pass}
CUTS = {"low-cut", "high-cut", "band-pass", "notch"}


# --- Report (the Engine tests' error measure) ----------------------------------------------------

def position_band(frequency, fs):
    """Where Frequency sits relative to Nyquist, in the bands the Engine tests' tolerances use."""
    position = frequency / (fs / 2)
    return "<=0.45" if position <= 0.45 else "<=0.73" if position <= 0.73 else "<=0.91"


def report(shape, sample_rates=(44100, 48000, 96000),
           frequencies=(20, 200, 1000, 2000, 5000, 9000, 10000, 15000, 18000, 20000),
           gains=(-30, -18, -12, -6, -3, 3, 12, 18, 30), qs=(0.71,), orders=(2,), points=200):
    """Worst error per (position band, Q), as dB and as a share of the target's span in dB: how far it
    strays from its value at 10 Hz, and at least |Gain|, as in the Engine tests."""
    worst = {}
    for fs in sample_rates:
        for frequency in frequencies:
            if frequency > 0.91 * fs / 2:
                continue
            for gain in gains:
                for q in qs:
                    for order in orders:
                        sections, target = shape(fs, frequency, gain, q, order)
                        error, span, lowest = 0.0, abs(gain), target(10)
                        for i in range(points + 1):
                            f = 10 * (fs / 2 / 10) ** (i / points)
                            t = target(f)
                            span = max(span, abs(t - lowest))
                            error = max(error, abs(cascade_db(sections, f, fs) - t))
                        share = error / max(span, 1e-9)
                        key = (position_band(frequency, fs), q)
                        if share > worst.get(key, (0,))[0]:
                            worst[key] = (share, error, fs, frequency, gain, order)
    print(f"{'band':>7} {'Q':>6}  {'share':>6} {'dB':>6}   worst case")
    for (band, q), (share, error, fs, frequency, gain, order) in sorted(worst.items()):
        print(f"{band:>7} {q:>6}  {share:6.1%} {error:6.2f}   fs={fs} F={frequency} G={gain} order={order}")


def report_cut(shape, sample_rates=(44100, 48000, 96000),
               frequencies=(20, 200, 2000, 9000, 15000, 20000), qs=(0.71,), orders=(2,), points=200):
    """Worst error per (position band, Q) in dB, against the target shifted up to 1/12 octave either
    way: above it (louder, or above -60 dB where the target is below) and below it (where the target
    is above -24 dB). Order 32 is Brickwall."""
    worst = {}
    r = 2 ** (1 / 12)
    for fs in sample_rates:
        for frequency in frequencies:
            if frequency > 0.91 * fs / 2:
                continue
            for q in qs:
                for order in orders:
                    sections, target = shape(fs, frequency, None, q, order)
                    for i in range(points + 1):
                        f = 10 * (fs / 2 / 10) ** (i / points)
                        near = [target(f / r), target(f), target(f * r)]
                        h = 1
                        for s in sections:
                            h *= s.response(2 * PI * f / fs)
                        measured = 20 * math.log10(max(abs(h), 1e-30))
                        above = measured - max(max(near), -60)
                        below = min(near) - measured if min(near) > -24 else 0
                        w = worst.setdefault((position_band(frequency, fs), q), [0, 0])
                        w[0], w[1] = max(w[0], above), max(w[1], below)
    print(f"{'band':>7} {'Q':>6}  {'above':>6} {'below':>6}")
    for (band, q), (above, below) in sorted(worst.items()):
        print(f"{band:>7} {q:>6}  {above:6.2f} {below:6.2f}")


def report_phase(shape, sample_rates=(44100, 48000, 96000),
                 frequencies=(20, 200, 2000, 9000, 15000, 20000), qs=(0.71,), orders=(1,), points=200):
    """All Pass: worst phase error in degrees, wrapped to +/-180, from 10 Hz up to Frequency, per
    (position band, order); and the worst magnitude error in dB anywhere."""
    worst, flat = {}, 0.0
    for fs in sample_rates:
        for frequency in frequencies:
            if frequency > 0.91 * fs / 2:
                continue
            for q in qs:
                for order in orders:
                    sections, phase = shape(fs, frequency, None, q, order)
                    for i in range(points + 1):
                        f = 10 * (frequency / 10) ** (i / points) if i < points else frequency
                        h = 1
                        for s in sections:
                            h *= s.response(2 * PI * f / fs)
                        flat = max(flat, abs(20 * math.log10(abs(h))))
                        error = abs(math.degrees((cmath.phase(h) - phase(f) + PI) % (2 * PI) - PI))
                        key = (position_band(frequency, fs), order)
                        worst[key] = max(worst.get(key, 0), error)
    print(f"magnitude flat within {flat:.2e} dB")
    print(f"{'band':>7} {'order':>5}  {'degrees':>7}")
    for (band, order), error in sorted(worst.items()):
        print(f"{band:>7} {order:>5}  {error:7.2f}")


def shape_options(parser):
    parser.add_argument("--q", type=float, nargs="+", default=[0.71])
    parser.add_argument("--orders", type=int, nargs="+", default=[2])
    parser.add_argument("--slopes", type=float, nargs="+",
                        help="fractional Slopes in dB/oct (#18): the morphing cascade instead of --orders")
    parser.add_argument("--target", choices=TARGETS, default="partial", help="the candidate target for --slopes")


@command(*SHAPES, options=shape_options)
def run_shape(args):
    reporter = report_phase if args.command == "all-pass" else report_cut if args.command in CUTS else report
    if not args.slopes:
        reporter(SHAPES[args.command], qs=args.q, orders=args.orders)
    elif args.command in FRACTIONAL_SHAPES:
        reporter(fractional_shape(args.command, args.target), qs=args.q, orders=args.slopes)
    else:
        raise SystemExit(f"--slopes works on {', '.join(FRACTIONAL_SHAPES)}")


# --- Fractional Slopes (#18): candidate targets and one morphing cascade ------------------------
# A Slope between whole orders N and N + 1 (order nu = N + t; Notch counts 12 dB/oct per order). The
# candidate targets:
#   blend:   the neighbouring whole-order targets blended in dB: (1 - t) x order N + t x order N + 1.
#   partial: the morphing cascade below is itself the target.
#   formula: the Butterworth magnitude with a fractional exponent, |H|^2 = 1 / (1 + x^(2 nu)) (Shelves:
#            (g + x^(2 nu)) / (1/g + x^(2 nu))); it has no Q, so it is defined at Q 0.71 only.
# All are realised by the same morphing cascade, so they cost the same: order N + 1's sections.
#
# The morphing cascade: order N + 1's sections, each second-order section's damping moving linearly with
# t from order N's Butterworth value to order N + 1's (Q scaling applied at both ends). The section order
# N lacks grows in as a partial section: for N even, a first-order pole coming in from infinity (ts + 1);
# for N odd, order N's real pole gaining a second pole from infinity (ts^2 + ds + 1), the two meeting and
# becoming a pair before t reaches 1. The cascade is then rescaled in frequency so its corner stays at
# Frequency (-3 dB for Cuts, half the Gain in dB for Shelves), measured at Q 0.71; below order 1 the pole
# just comes in from infinity, unscaled. At t = 0 it is exactly order N's Butterworth cascade, and every
# coefficient moves continuously with Slope. Proposed in docs/adr/0006-fractional-slopes-morph-between-whole-orders.md.

def _lerp(a, b, t):
    return a + (b - a) * t


def _critical_share(d1_hi):
    """The t at which ts^2 + lerp(1, d1_hi, t)s + 1 has a double root."""
    lo, hi = 0.0, 1.0
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        lo, hi = (mid, hi) if _lerp(1.0, d1_hi, mid) ** 2 > 4 * mid else (lo, mid)
    return hi


def _morph_prototype(nu, q):
    """The morphing low-pass prototype's sections before rescaling, each with its emerging share: None for
    a section whose roots are all whole, else the share (0 to 1) of a whole root's step a Shelf gives the
    root coming in from infinity, reaching 1 where it meets order N's real pole."""
    n = math.floor(nu)
    t = nu - n
    lo = [a for a in _butterworth_sections(n, q) if a.d2]
    hi = [a for a in _butterworth_sections(n + 1, q) if a.d2]
    out = [(Analog(0, 0, 1, 1, _lerp(a.d1, b.d1, t), 1), None) for a, b in zip(lo, hi)]
    if n % 2 == 0:
        if t > 0:
            out.append((Analog(0, 0, 1, 0, t, 1), t))
    else:
        crit = _critical_share(hi[-1].d1)
        out.append((Analog(0, 0, 1, t, _lerp(1.0, hi[-1].d1, t), 1), t / crit if 0 < t < crit else None))
    return out


def _scaled(a: Analog, c):
    """The section with s -> c s."""
    return Analog(a.n2 * c * c, a.n1 * c, a.n0, a.d2 * c * c, a.d1 * c, a.d0)


def _crossing(f, lo=0.01, hi=100.0):
    """The x in [lo, hi] where f goes from positive to negative, by bisection in log x."""
    for _ in range(80):
        mid = math.sqrt(lo * hi)
        lo, hi = (mid, hi) if f(mid) > 0 else (lo, mid)
    return math.sqrt(lo * hi)


def morph_low_pass(nu, q):
    """The morphing cascade's low-pass sections, -3 dB at s = 1 at Q 0.71, with the same rescaling at
    any Q (so a whole order is exactly order N's Butterworth at every Q)."""
    sections = [a for a, _ in _morph_prototype(nu, q)]
    if nu == math.floor(nu) or nu < 1:  # below order 1 the pole comes in from infinity: no corner to keep
        return sections
    plain = [a for a, _ in _morph_prototype(nu, math.sqrt(0.5))]
    c = _crossing(lambda x: _analog_db(plain, x, 1) + 10 * math.log10(2))
    return [_scaled(a, c) for a in sections]


def _roots(a: Analog):
    """A prototype section's poles: the upper-half-plane one of a pair, else the real ones."""
    if a.d2 == 0:
        return [complex(-a.d0 / a.d1)] if a.d1 else []
    disc = a.d1 * a.d1 - 4 * a.d2 * a.d0
    if disc < 0:
        return [complex(-a.d1, math.sqrt(-disc)) / (2 * a.d2)]
    r = math.sqrt(disc)
    return [complex((-a.d1 + r) / (2 * a.d2)), complex((-a.d1 - r) / (2 * a.d2))]


def _monic_at_dc(roots):
    """prod (1 - s/r) as (c2, c1, c0); an upper-half-plane root stands for its conjugate pair."""
    if len(roots) == 2:
        a, b = roots[0].real, roots[1].real
        return 1 / (a * b), -(1 / a + 1 / b), 1.0
    r = roots[0]
    if r.imag:
        return abs(1 / r) ** 2, -2 * (1 / r).real, 1.0
    return 0.0, -1 / r.real, 1.0


def _morph_low_shelf_unscaled(nu, dc_gain, q):
    """_morph_low_shelf_analog's sections before the corner rescale."""
    out, shares = [], []
    for a, share in _morph_prototype(nu, q):
        rs = _roots(a)
        if share is None:
            ws = [1.0] * len(rs)
        elif len(rs) == 1:
            ws = [share]
        else:  # two real poles: the one nearer the origin is order N's real pole
            ws = [1.0, share] if abs(rs[0]) < abs(rs[1]) else [share, 1.0]
        if rs:
            out.append(rs)
            shares.append(ws)
    total = sum(w * (2 if r.imag else 1) for rs, ws in zip(out, shares) for r, w in zip(rs, ws))
    rz = dc_gain ** (1 / (2 * total))
    sections = [Analog(*_monic_at_dc([r * rz ** w for r, w in zip(rs, ws)]),
                       *_monic_at_dc([r / rz ** w for r, w in zip(rs, ws)])) for rs, ws in zip(out, shares)]
    first = sections[0]
    sections[0] = Analog(first.n2 * dc_gain, first.n1 * dc_gain, first.n0 * dc_gain, first.d2, first.d1, first.d0)
    return sections


def _morph_low_shelf_analog(nu, dc_gain, q):
    """Low Shelf of fractional order: each prototype pole r gives a zero at r x rz^w and a pole at r / rz^w,
    w its share (1 for a whole root), with rz^(2 sum w) = dc_gain, so a root coming in from infinity
    brings its step in gradually. Rescaled so it reaches half its Gain in dB at s = 1, at Q 0.71."""
    sections = _morph_low_shelf_unscaled(nu, dc_gain, q)
    if nu == math.floor(nu) or dc_gain == 1:
        return sections
    plain, half = _morph_low_shelf_unscaled(nu, dc_gain, math.sqrt(0.5)), 10 * math.log10(dc_gain)
    sign = 1 if dc_gain > 1 else -1
    c = _crossing(lambda x: sign * (_analog_db(plain, x, 1) - half))
    return [_scaled(a, c) for a in sections]


def _product(a: Biquad, b: Biquad):
    """Two first-order sections as one biquad."""
    return Biquad(a.b0 * b.b0, a.b0 * b.b1 + a.b1 * b.b0, a.b1 * b.b1, a.a1 + b.a1, a.a1 * b.a1)


def _fractional_cut(shape, fs, frequency, q, nu):
    """high_cut's and low_cut's designs on the morphing prototype. A section whose two poles are real
    (the partial section before its poles meet) is designed as two first-order sections in one biquad:
    as one section, its far pole would be held below Nyquist with the near one."""
    proto = morph_low_pass(nu, q)
    high = shape == "high-cut"
    if not high:  # s -> 1/s: 1 / (d2 s^2 + d1 s + 1) -> s^2 / (s^2 + d1 s + d2)
        target = [Analog(0, 1, 0, 0, 1, a.d1) if a.d2 == 0 else Analog(1, 0, 0, 1, a.d1, a.d2) for a in proto]
    else:
        target = proto

    def first_order(pole):  # a real prototype pole -a
        a = -pole.real
        return match_section(Analog(0, 0, 1, 0, 1 / a, 1), frequency, fs) if high else \
            match_high_pass(Analog(0, 1, 0, 0, 1, 1 / a), frequency, fs)

    sections = []
    for a, t in zip(proto, target):
        roots = _roots(a)
        if len(roots) == 1 and not roots[0].imag:
            sections.append(first_order(roots[0]))
        elif len(roots) == 2:
            sections.append(_product(first_order(roots[0]), first_order(roots[1])))
        elif not high:
            sections.append(match_high_pass(t, frequency, fs))
        else:
            b, w0 = normalised(Analog(0, 0, 1 / a.d2, 1, a.d1 / a.d2, 1 / a.d2))
            damped = frequency * w0 * math.sqrt(max(1 - b.d1 * b.d1 / 4, 0.01))
            sections.append(match_section(b, frequency * w0, fs, match_hz=min(damped, fs / 4)))
    return sections, lambda f: _analog_db(target, f, frequency)


def _fractional_band_pass(fs, frequency, q, nu):
    """band_pass's design on the morphing prototype: a real prototype pole gives one band-pass section,
    a pair a lower (Low Cut) and an upper (High Cut) section. Steep from ceil(nu) >= 5."""
    steep = math.ceil(nu) >= STEEP_BAND_PASS_ORDER
    hold = BAND_PASS_HOLD * fs / 2
    analog, sections = [], []
    for a in morph_low_pass(nu, math.sqrt(0.5)):
        for p in _roots(a):
            if not p.imag:
                d = -p.real / q
                s = Analog(0, d, 0, 1, d, 1)
                analog.append(s)
                sections.append(match_section(s, frequency, fs))
                continue
            low, high = sorted([(-2 * x.real, abs(x) ** 2) for x in _reciprocal_roots(p / q)], key=lambda d: d[1])
            lower, w_low = normalised(Analog(1, 0, 0, 1, *low))
            upper, w_high = normalised(Analog(0, 0, 1 / (q * q), 1, *high))
            analog += [Analog(1, 0, 0, 1, *low), Analog(0, 0, 1 / (q * q), 1, *high)]
            sections.append(match_high_pass(lower, frequency * w_low, fs, and_nyquist=True))
            reference = frequency * w_high
            damped = math.sqrt(max(1 - upper.d1 * upper.d1 / 4, 0.01))
            match = min(reference * min(damped, 0.9), fs / 4)
            if steep and reference > hold:
                sections.append(_held_upper_section(upper, reference, hold, match, fs))
            else:
                sections.append(match_section(upper, reference, fs, match_hz=match))
    if steep:
        at = min(frequency, hold)
        k = 10 ** ((_analog_db(analog, at, frequency) - cascade_db(sections, at, fs)) / 20)
        s = sections[0]
        s.b0, s.b1, s.b2 = s.b0 * k, s.b1 * k, s.b2 * k
    return sections, lambda f: _analog_db(analog, f, frequency)


def _fractional_notch(fs, frequency, q, nu):
    """notch's design on the morphing prototype: every section's zeros exactly at Frequency."""
    analog, sections = [], []
    for a in morph_low_pass(nu, math.sqrt(0.5)):
        for p in _roots(a):
            xs = _reciprocal_roots(1 / (p * q))
            quads = [(-2 * x.real, abs(x) ** 2) for x in xs] if p.imag else [(-(xs[0] + xs[1]).real, 1.0)]
            for d1, d0 in quads:
                s = Analog(1, 0, 1, 1, d1, d0)
                b, w0 = normalised(s)
                analog.append(s)
                sections.append(match_notch(b, frequency * w0, fs, frequency))
    return sections, lambda f: _analog_db(analog, f, frequency)


def _fractional_shelf(shape, fs, frequency, gain, q, nu):
    """low_shelf's, high_shelf's and tilt_shelf's designs on the morphing Low Shelf."""
    order = 2 * sum(1 for a, _ in _morph_prototype(nu, q) if a.d2)  # _shelf_sections reads the pairs from it
    boost = _morph_low_shelf_analog(nu, 10 ** (abs(gain) / 20), q)
    if shape == "low-shelf":
        target = _morph_low_shelf_analog(nu, 10 ** (gain / 20), q)
        return _shelf_sections(boost, frequency, fs, q, order, gain < 0), lambda f: _analog_db(target, f, frequency)
    sections = _shelf_sections(map(_mirrored_cut, boost), frequency, fs, q, order, gain >= 0)
    # The Low Shelf boost mirrored (s -> 1/s), inverted for a cut. (Unlike Butterworth's, a fractional
    # Low Shelf of -Gain, moved up by Gain, isn't its mirror image.)
    sign = 1 if gain >= 0 else -1
    shift = -gain / 2 if shape == "tilt-shelf" else 0.0
    k = 10 ** (shift / 20)
    first = sections[0]
    sections[0] = Biquad(first.b0 * k, first.b1 * k, first.b2 * k, first.a1, first.a2)
    return sections, lambda f: shift + sign * _analog_db(boost, frequency * frequency / f, frequency)


FRACTIONAL_SHAPES = ("low-shelf", "high-shelf", "tilt-shelf", "low-cut", "high-cut", "band-pass", "notch")
SHELVES = ("low-shelf", "high-shelf", "tilt-shelf")
TARGETS = ("blend", "partial", "formula")


def _order(shape, slope):
    return slope / (12 if shape == "notch" else 6)


def fractional_design(shape, fs, frequency, gain, q, slope):
    """The morphing cascade for shape at slope in dB/oct: (digital sections, its analog response in dB,
    which is the partial target). A Cut or Band Pass below 6 dB/oct grows in from passing unchanged.
    slope-gaps' "before"; the fractional reports design with continuous_design."""
    nu = _order(shape, slope)
    if nu == 0:
        return [], lambda f: 0.0
    if shape in SHELVES:
        return _fractional_shelf(shape, fs, frequency, gain, q, nu)
    if shape == "band-pass":
        return _fractional_band_pass(fs, frequency, q, nu)
    if shape == "notch":
        return _fractional_notch(fs, frequency, q, nu)
    return _fractional_cut(shape, fs, frequency, q, nu)


def _whole(shape, fs, frequency, gain, q, order):
    """The existing whole-order target; a Cut or Band Pass of order 0 passes the signal unchanged."""
    return (lambda f: 0.0) if order == 0 else SHAPES[shape](fs, frequency, gain, q, order)[1]


def _formula_db(nu, x):
    return -10 * math.log10(1 + x ** (2 * nu))


def _formula_low_shelf_db(nu, gain, x):
    g = 10 ** (gain / 20)
    xx = x ** (2 * nu)
    return 10 * math.log10((g + xx) / (1 / g + xx))


def fractional_target(target, shape, fs, frequency, gain, q, slope):
    """A candidate target, target(f) in dB, for shape at slope in dB/oct."""
    nu = _order(shape, slope)
    n, t = math.floor(nu), nu - math.floor(nu)
    if target == "partial":
        return continuous_design(shape, fs, frequency, gain, q, slope)[1]
    if target == "blend":
        lo = _whole(shape, fs, frequency, gain, q, n)
        hi = _whole(shape, fs, frequency, gain, q, n + 1) if t else lo
        return lambda f: (1 - t) * lo(f) + t * hi(f)
    if nu == 0:
        return lambda f: 0.0
    band = lambda f: q * abs(f / frequency - frequency / f)
    return {
        "high-cut": lambda f: _formula_db(nu, f / frequency),
        "low-cut": lambda f: _formula_db(nu, frequency / f),
        "band-pass": lambda f: _formula_db(nu, band(f)),
        "notch": lambda f: _formula_db(nu, 1 / max(band(f), 1e-300)),
        "low-shelf": lambda f: _formula_low_shelf_db(nu, gain, f / frequency),
        "high-shelf": lambda f: gain + _formula_low_shelf_db(nu, -gain, f / frequency),
        "tilt-shelf": lambda f: gain / 2 + _formula_low_shelf_db(nu, -gain, f / frequency),
    }[shape]


def fractional_shape(shape, target):
    """A shape for report() and report_cut(): its order argument is the Slope in dB/oct, its design the
    morphing cascade (continuous_design), and its target the candidate."""
    def design(fs, frequency, gain, q, slope):
        sections, _ = continuous_design(shape, fs, frequency, gain, q, slope)
        return sections, fractional_target(target, shape, fs, frequency, gain, q, slope)
    return design


def _steepness(shape, curve, frequency, gain):
    """Values that must not rise as Slope rises: the level in a Cut's or Band Pass's stopband, a Notch's
    level inside its band, and how far a Shelf is from the plateau it is approaching on each side."""
    if shape == "notch":
        return [curve(frequency * r) for r in (1.05, 1.1)]
    if shape == "band-pass":
        return [curve(frequency * r) for r in (2.5, 3, 4)] + [curve(frequency / r) for r in (2.5, 3, 4)]
    if shape == "low-cut":
        return [curve(frequency / r) for r in (1.25, 1.5, 2)]
    if shape == "high-cut":
        return [curve(frequency * r) for r in (1.25, 1.5, 2)]
    high, low = gain, 0.0  # the plateaus above and below Frequency
    if shape == "low-shelf":
        high, low = 0.0, gain
    if shape == "tilt-shelf":
        high, low = gain / 2, -gain / 2
    return ([abs(curve(frequency * r) - high) for r in (1.25, 1.5, 2)] +
            [abs(curve(frequency / r) - low) for r in (1.25, 1.5, 2)])


def _bump(values):
    """How far a curve that should never rise rises above the lowest it has been, in dB."""
    lowest, bump = math.inf, 0.0
    for v in values:
        lowest = min(lowest, v)
        bump = max(bump, v - lowest)
    return bump


def _bumps(shape, curve, frequency, gain, grid):
    """The largest bump or dip against the way the curve should run: High Cut down, Low Cut up, Band
    Pass up then down, Notch down then up, Shelves from one plateau to the other."""
    below = [curve(f) for f in grid if f < frequency]
    above = [curve(f) for f in grid if f >= frequency]
    if shape in SHELVES:
        rising = (gain > 0) != (shape == "low-shelf")
        values = below + above
        return _bump([-v for v in values] if rising else values)
    if shape == "high-cut":
        return _bump(below + above)
    if shape == "low-cut":
        return _bump([-v for v in below + above])
    flip = -1 if shape == "band-pass" else 1
    return max(_bump([flip * v for v in below]), _bump([-flip * v for v in above]))


@command("slopes")
def run_slopes(args):
    report_slopes()


def report_slopes(frequency=1000.0, fs=1e8, gains=(-18.0, 6.0, 18.0), q=math.sqrt(0.5), step=0.25):
    """Each candidate target's behaviour as Slope moves in step dB/oct from the Shape's minimum to 96,
    at Q 0.71, analog only:
    - whole: the largest difference from the existing Butterworth target at whole orders, dB;
    - steeper: how far the curve ever gets less steep as Slope rises (_steepness), dB; 0 is monotonic;
    - bump: the largest bump or dip against the way the curve should run (_bumps), dB;
    - vs partial: the morphing cascade's largest difference from the target, where either is above
      -60 dB (the Engine tests' floor), dB."""
    grid = [10 * 2 ** (i / 48) for i in range(48 * 14)]
    print(f"{'shape':>10} {'target':>8}  {'whole':>7} {'steeper':>7} {'bump':>6} {'vs partial':>10}")
    for shape in FRACTIONAL_SHAPES:
        unit = 12 if shape == "notch" else 6
        lowest = 0 if shape in ("low-cut", "high-cut", "band-pass") else unit
        slopes = [lowest + i * step for i in range(int(round((96 - lowest) / step)) + 1)]
        for target in TARGETS:
            whole, steeper, bump, vs = 0.0, 0.0, 0.0, 0.0
            for gain in gains if shape in SHELVES else (0.0,):
                previous = None
                for slope in slopes:
                    curve = fractional_target(target, shape, fs, frequency, gain, q, slope)
                    values = [curve(f) for f in grid]
                    if slope % unit == 0:
                        ref = _whole(shape, fs, frequency, gain, q, int(slope // unit))
                        whole = max(whole, max(abs(v - ref(f)) for v, f in zip(values, grid)))
                    probe = _steepness(shape, curve, frequency, gain)
                    if previous is not None:
                        steeper = max([steeper] + [p - pp for p, pp in zip(probe, previous)])
                    previous = probe
                    bump = max(bump, _bumps(shape, curve, frequency, gain, grid))
                    if target != "partial":
                        part = fractional_target("partial", shape, fs, frequency, gain, q, slope)
                        vs = max([vs] + [abs(part(f) - v) for v, f in zip(values, grid) if max(v, part(f)) > -60])
            print(f"{shape:>10} {target:>8}  {whole:7.0e} {steeper:7.3f} {bump:6.3f} {vs:10.2f}")


# --- Fractional Slope gaps (ADR 0006): seams, resonant and High Shelf tolerances, the corner rescale -
# The continuous design: fractional_design with these changes. Each touches only the partial section, a
# Shelf's resonant split or Band Pass, and whole orders stay exactly as they are (within 1e-13 dB).
#   - High Cut: the partial section, real poles or a pair, is one matched biquad with exact poles. While
#     its poles are real, its match point aims at a blend of the analog and two first-order sections'
#     response, weighted by how close its poles are (_closeness: 0 as one comes in from infinity, 1 where
#     they meet). No seam where they meet, and none as the pole comes in.
#   - Shelves: a section's resonant split reduces its zeros' Q to what the same section has at Q 2 (for
#     whole orders, exactly the existing (2 / Q)^(1 / pairs)); the partial section's split is weighted by
#     t^PARTIAL_SPLIT_POWER, and while its roots are real it is matched as the High Cut's is.
#   - Band Pass: a pair's upper section gets the transform's gain, |p|^2 / Q^2, in the target too; the
#     steep design comes in over 24 to 30 dB/oct instead of switching on above 24.
# Low Cut and Notch keep fractional_design's design. Band Pass and Notch keep one seam each where the
# partial section's poles meet: the prototype's real poles and its pair are grouped into different
# sections there. The Engine crossfades across it, as it does for every Slope change today.
# The corner rescale is CORNER_NEWTON_STEPS Newton steps in ln(frequency), from the whole-order corner.

PARTIAL_SPLIT_POWER = 3
CORNER_NEWTON_STEPS = 3


def _level_and_slope(sections, x):
    """The analog cascade's level at s = jx in dB, and its derivative in dB per unit of ln x."""
    s, level, slope = 1j * x, 0.0, 0.0
    for a in sections:
        n, d = a.n2 * s * s + a.n1 * s + a.n0, a.d2 * s * s + a.d1 * s + a.d0
        level += 20 * math.log10(abs(n / d))
        slope += 20 / math.log(10) * (s * (2 * a.n2 * s + a.n1) / n - s * (2 * a.d2 * s + a.d1) / d).real
    return level, slope


def newton_corner(sections, level_db, steps=CORNER_NEWTON_STEPS):
    """The x where the cascade is at level_db, by a fixed number of Newton steps in ln x from x = 1 (where a
    whole order has it)."""
    y = 0.0
    for _ in range(steps):
        level, slope = _level_and_slope(sections, math.exp(y))
        y -= (level - level_db) / slope
    return math.exp(y)


def _corner_problem(shape, nu, gain):
    """The unscaled Q 0.71 cascade whose corner the rescale finds, and the level it is found at."""
    if shape in SHELVES:
        g = 10 ** (gain / 20)
        return _morph_low_shelf_unscaled(nu, g, math.sqrt(0.5)), 10 * math.log10(g)
    return [a for a, _ in _morph_prototype(nu, math.sqrt(0.5))], -10 * math.log10(2)


def _real_pairs(a: Analog):
    """A second-order section with real zeros (if any) and real poles as two first-order sections, zeros
    and poles paired by size; its gain goes with the first."""
    def real_roots(c2, c1, c0):
        r = math.sqrt(max(c1 * c1 - 4 * c2 * c0, 0))
        return sorted([(-c1 + r) / (2 * c2), (-c1 - r) / (2 * c2)], key=abs)
    poles = real_roots(a.d2, a.d1, a.d0)
    zeros = real_roots(a.n2, a.n1, a.n0) if a.n2 else [None, None]
    k = a.n0 / a.d0
    out = []
    for z, p in zip(zeros, poles):
        out.append(Analog(0, -k / z if z else 0, k, 0, -1 / p, 1))
        k = 1
    return out


def _closeness(a: Analog):
    """For real poles, 1 - (1 - near / far)^2: 1 where they meet, 0 as one comes in from infinity, and
    smooth where they meet (near / far alone moves as the square root of the distance). 1 for a pair."""
    disc = a.d1 * a.d1 - 4 * a.d2 * a.d0
    if disc < 0:
        return 1.0
    r = math.sqrt(disc)
    return 1 - (2 * r / (a.d1 + r)) ** 2


def _partial_biquad(a: Analog, analog_at_match: Analog, frequency, fs, match_hz):
    """One matched biquad with a's poles; at DC, Nyquist and match_hz it meets analog_at_match's
    magnitude; at match_hz, times (while a's poles are real) two first-order sections' error there,
    weighted by 1 - _closeness(a)."""
    x = match_hz / frequency
    at_match = analog_at_match.squared(x)
    beta = _closeness(a)
    if beta < 1:
        w = 2 * PI * match_hz / fs
        first, second = (match_section(s, frequency, fs) for s in _real_pairs(a))
        at_match *= (abs(first.response(w) * second.response(w)) ** 2 / a.squared(x)) ** (1 - beta)
    return match_magnitudes(a, frequency, fs, match_hz, analog_at_match.squared(0), at_match,
                            analog_at_match.squared(fs / 2 / frequency))


def _continuous_high_cut(fs, frequency, q, nu):
    proto = morph_low_pass(nu, q)
    n, t = math.floor(nu), nu - math.floor(nu)
    sections, target = _fractional_cut("high-cut", fs, frequency, q, nu)
    if n % 2 and t > 0:
        a = proto[-1]
        w0 = math.sqrt(a.d0 / a.d2)
        damped = frequency * w0 * math.sqrt(max(1 - a.d1 * a.d1 / (4 * a.d0 * a.d2), 0.01))
        sections[-1] = _partial_biquad(a, a, frequency, fs, min(damped, fs / 4))
    return sections, target


def _split_toward(a: Analog, reference_q, frequency, fs, q, weight, partial):
    """_resonant_split, with the zeros' Q reduced toward reference_q (the section's at Q 2) and the split
    scaled by weight; a partial section with real roots is matched by _partial_biquad."""
    natural = math.sqrt(a.n0 / a.n2)
    zeros_q = math.sqrt(a.n0 * a.n2) / a.n1
    x = natural * frequency / (fs / 2)
    t = math.log(SPLIT_NONE / x) / math.log(SPLIT_NONE / SPLIT_FULL)
    t = _smoothstep(t) * _smoothstep(math.log(q / RESONANT_SHELF_Q) / math.log(SPLIT_IN / RESONANT_SHELF_Q)) if q > RESONANT_SHELF_Q else 0.0
    t *= weight * (_closeness(a) if partial else 1.0)
    split_q = zeros_q * min(reference_q / zeros_q, 1.0) ** t
    resonance = match_section(Analog(1, 1 / split_q, 1, 1, 1 / zeros_q, 1), frequency * natural, fs)
    split = Analog(a.n2, math.sqrt(a.n0 * a.n2) / split_q, a.n0, a.d2, a.d1, a.d0)
    poles = _partial_biquad(a, split, frequency, fs, frequency) if partial else match_section(split, frequency, fs)
    if split_q >= zeros_q:
        resonance.b0, resonance.b1, resonance.b2 = 1, resonance.a1, resonance.a2
        return [poles, resonance]
    return [poles, inverse(resonance)]


def _continuous_shelf(shape, fs, frequency, gain, q, nu):
    n, t = math.floor(nu), nu - math.floor(nu)
    g = 10 ** (abs(gain) / 20)
    boost = _morph_low_shelf_analog(nu, g, q)
    reference = _morph_low_shelf_analog(nu, g, min(q, RESONANT_SHELF_Q))
    if shape != "low-shelf":
        boost, reference = list(map(_mirrored_cut, boost)), list(map(_mirrored_cut, reference))
    invert = gain < 0 if shape == "low-shelf" else gain >= 0
    sections = []
    for i, (s, r) in enumerate(zip(boost, reference)):
        if s.d2 == 0:
            sections.append(inverse(match_section(s, frequency, fs)) if invert else match_section(s, frequency, fs))
            continue
        partial = n % 2 == 1 and t > 0 and i == len(boost) - 1
        poles, zeros = _split_toward(s, math.sqrt(r.n0 * r.n2) / r.n1, frequency, fs, q,
                                     t ** PARTIAL_SPLIT_POWER if partial else 1.0, partial)
        sections += [inverse(poles), inverse(zeros)] if invert else [zeros, poles]
    _, target = _fractional_shelf(shape, fs, frequency, gain, q, nu)
    if shape == "tilt-shelf":
        k = 10 ** (-gain / 40)
        first = sections[0]
        sections[0] = Biquad(first.b0 * k, first.b1 * k, first.b2 * k, first.a1, first.a2)
    return sections, target


def _lerp_biquad(a: Biquad, b: Biquad, t):
    return Biquad(*(_lerp(x, y, t) for x, y in zip((a.b0, a.b1, a.b2, a.a1, a.a2), (b.b0, b.b1, b.b2, b.a1, b.a2))))


def _continuous_band_pass(fs, frequency, q, nu):
    """_fractional_band_pass with two changes:
    - a pair's upper section has the gain the transform gives it, |p|^2 / Q^2. fractional_design's 1 / Q^2
      is right only for Butterworth's |p| = 1: its target's passband fell by up to 7.7 dB, and jumped by
      that where the partial section's poles meet;
    - the steep design comes in over orders 4 to 5: each held upper section's coefficients and the gain
      correction move linearly from the plain design (stable throughout, as a biquad's stable
      coefficients form a convex set)."""
    steepness = min(max(nu - (STEEP_BAND_PASS_ORDER - 1), 0.0), 1.0)
    hold = BAND_PASS_HOLD * fs / 2

    def lower_upper(low, high, upper_gain):
        """A Low Cut section for the quadratic low, a High Cut section for high, as band_pass designs them."""
        lower, w_low = normalised(Analog(1, 0, 0, 1, *low))
        upper, w_high = normalised(Analog(0, 0, upper_gain, 1, *high))
        reference = frequency * w_high
        damped = math.sqrt(max(1 - upper.d1 * upper.d1 / 4, 0.01))
        match = min(reference * min(damped, 0.9), fs / 4)
        plain = match_section(upper, reference, fs, match_hz=match)
        if steepness and reference > hold:
            plain = _lerp_biquad(plain, _held_upper_section(upper, reference, hold, match, fs), steepness)
        return [match_high_pass(lower, frequency * w_low, fs, and_nyquist=True), plain]

    analog, sections = [], []
    for a in morph_low_pass(nu, math.sqrt(0.5)):
        roots = _roots(a)
        if roots[0].imag:
            p = roots[0]
            low, high = sorted([(-2 * x.real, abs(x) ** 2) for x in _reciprocal_roots(p / q)], key=lambda d: d[1])
            gain = abs(p) ** 2 / (q * q)
            analog += [Analog(1, 0, 0, 1, *low), Analog(0, 0, gain, 1, *high)]
            sections += lower_upper(low, high, gain)
            continue
        ds = [-p.real / q for p in roots]
        singles = [Analog(0, d, 0, 1, d, 1) for d in ds]
        analog += singles
        sections += [match_section(s, frequency, fs) for s in singles]
    if steepness:
        at = min(frequency, hold)
        k = 10 ** (steepness * (_analog_db(analog, at, frequency) - cascade_db(sections, at, fs)) / 20)
        s = sections[0]
        s.b0, s.b1, s.b2 = s.b0 * k, s.b1 * k, s.b2 * k
    return sections, lambda f: _analog_db(analog, f, frequency)


def continuous_design(shape, fs, frequency, gain, q, slope):
    """fractional_design with the changes above, and its section count."""
    nu = _order(shape, slope)
    if nu == 0:
        return [], lambda f: 0.0
    if shape in SHELVES:
        return _continuous_shelf(shape, fs, frequency, gain, q, nu)
    if shape == "band-pass":
        return _continuous_band_pass(fs, frequency, q, nu)
    if shape == "high-cut":
        return _continuous_high_cut(fs, frequency, q, nu)
    return fractional_design(shape, fs, frequency, gain, q, slope)


GAP_DESIGNS = {"before": fractional_design, "after": continuous_design}
SEAM_STEPS = (0.01, 0.001, 0.0001, 0.00001)  # dB/oct: the Slope changes the seams report looks at


def _seam_slopes(shape, q):
    """(kind, Slope, side): a pair of Slopes a step apart, starting side steps from Slope, at each place a
    seam can be: each whole order (either side), each place a partial section's poles meet (straddled),
    24 and 30 dB/oct (Band Pass's steep design) and every 1.5 dB/oct in between."""
    unit = 12 if shape == "notch" else 6
    lowest = 0 if shape in ("low-cut", "high-cut", "band-pass") else unit
    out = []
    for n in range(lowest // unit, 96 // unit + 1):
        whole = unit * n
        out += [("whole" if whole else "from 0", whole, side) for side in (-1, 0)
                if lowest < whole + side or (side == 0 and whole < 96)]
        if n % 2 and whole < 96:
            q_ = q if shape in SHELVES or shape in ("low-cut", "high-cut") else math.sqrt(0.5)
            pair = [a for a in _butterworth_sections(n + 1, q_) if a.d2][-1]
            out.append(("poles meet", unit * (n + _critical_share(pair.d1)), -0.5))
    if shape == "band-pass":
        out += [("24, 30", s, side) for s in (24, 30) for side in (-1, 0)]
    out += [("between", lowest + 0.75 + 1.5 * i, 0) for i in range(int((96 - lowest) / 1.5))]
    return out


def _seam_grid(shape):
    qs = (0.1, 0.71, 2, 10, 40) if shape in ("band-pass", "notch") else (0.71, 3, 10)
    gains = (-18, -3, 3, 18) if shape in SHELVES else (0,)
    return [(fs, frequency, gain, q) for fs in (44100, 48000, 96000) for frequency in (20, 200, 2000, 9000, 15000, 20000)
            if frequency <= 0.91 * fs / 2 for gain in gains for q in qs]


def _seam_job(job):
    """The worst seams for one (design, shape, fs, Frequency, Gain, Q): per kind, (excess at each of
    SEAM_STEPS, Slope, fs, Frequency, Gain, Q), the excess being how far the response's change over the
    step strays from the analog target's own change, in dB, where either response is above -60 dB. A
    seam's excess stays as the step shrinks; a continuous design's shrinks with it."""
    design, shape, fs, frequency, gain, q, points = job
    worst = {}
    grid = [10 * (fs / 2 / 10) ** (i / points) for i in range(points + 1)]
    for kind, slope, side in _seam_slopes(shape, q):
        excesses = []
        for step in SEAM_STEPS:
            start = max(slope + side * step, 0)
            (a, ta), (b, tb) = (GAP_DESIGNS[design](shape, fs, frequency, gain, q, s) for s in (start, start + step))
            excess = 0.0
            for f in grid:
                da, db = cascade_db(a, f, fs), cascade_db(b, f, fs)
                if max(da, db) >= -60:
                    excess = max(excess, abs(db - da - (tb(f) - ta(f))))
            excesses.append(excess)
        _keep_worst(worst, kind, (tuple(excesses), slope, fs, frequency, gain, q))
    return shape, design, worst


def _keep_worst(worst, key, value):
    """worst[key] = (each step's largest excess, and the place of the smallest step's)."""
    old = worst.get(key)
    if old is None:
        worst[key] = value
        return
    largest = tuple(map(max, old[0], value[0]))
    worst[key] = (largest,) + (value if value[0][-1] > old[0][-1] else old)[1:]


def _parallel(function, jobs, workers):
    if workers <= 1:
        return list(map(function, jobs))
    from concurrent.futures import ProcessPoolExecutor
    with ProcessPoolExecutor(workers) as pool:
        return list(pool.map(function, jobs, chunksize=1))


def report_seams(shapes, workers, points=100):
    """Seams, before and after: per Shape and kind of place, the worst excess over each of SEAM_STEPS, and
    the place whose excess over the smallest step is worst."""
    jobs = [(design, shape, *point, points) for shape in shapes for design in GAP_DESIGNS for point in _seam_grid(shape)]
    worst = {}
    for shape, design, found in _parallel(_seam_job, jobs, workers):
        for kind, value in found.items():
            _keep_worst(worst, (shape, kind, design), value)
    steps = " ".join(f"{s:>6g}" for s in SEAM_STEPS)
    print("seams: how far the response's change over a step in Slope (dB/oct) strays from the analog")
    print("target's own change, dB, worst over the grid. A seam stays as the step shrinks; 'after' is")
    print("a seam if its last step keeps over half the one before and over 0.01 dB. Each column is its")
    print("own worst case; the place shown is the smallest step's.")
    print(f"{'shape':>10} {'kind':>10}  before {steps}   after {steps}  verdict   worst after (before)")
    for shape in shapes:
        for kind in dict.fromkeys(k for s, k, _ in worst if s == shape):
            b, a = worst[(shape, kind, "before")], worst[(shape, kind, "after")]
            excesses, slope, fs, frequency, gain, q = a
            seam = excesses[-1] > 0.01 and excesses[-1] > 0.5 * excesses[-2]
            print(f"{shape:>10} {kind:>10}  {'':6} " + " ".join(f"{e:6.3f}" for e in b[0]) +
                  f"   {'':5} " + " ".join(f"{e:6.3f}" for e in excesses) +
                  f"  {'seam' if seam else 'none':7}   Slope={slope:.3f} fs={fs} F={frequency} G={gain:g} Q={q:g}"
                  f" (Slope={b[1]:.3f} fs={b[2]} F={b[3]} G={b[4]:g} Q={b[5]:g})")


SHELF_LIMITS = {"<=0.45": (0.12, 0.12), "<=0.73": (0.12, 0.15), "<=0.91": (0.45, 0.45)}  # (Q up to 2, above 2)


def _gap_slopes(shape, per_order=(0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 0.95)):
    """Slopes between whole orders, from the Shape's lowest whole order to 96 dB/oct."""
    unit = 12 if shape == "notch" else 6
    return [unit * (n + t) for n in range(1, 96 // unit) for t in per_order]


def _shelf_job(job):
    """report()'s worst share of the span per position band, for one (design, shape, fs, Frequency, Q)."""
    design, shape, fs, frequency, q, slopes, points = job
    worst = {}
    for gain in (-30, -18, -12, -6, -3, 3, 12, 18, 30):
        for slope in slopes:
            sections, target = GAP_DESIGNS[design](shape, fs, frequency, gain, q, slope)
            error, span, lowest = 0.0, abs(gain), target(10)
            for i in range(points + 1):
                f = 10 * (fs / 2 / 10) ** (i / points)
                t = target(f)
                span = max(span, abs(t - lowest))
                error = max(error, abs(cascade_db(sections, f, fs) - t))
            share = error / span
            key = position_band(frequency, fs)
            if share > worst.get(key, (-1,))[0]:
                worst[key] = (share, error, slope, fs, frequency, gain)
    return design, shape, q, worst


def report_shelves(shapes, qs, workers, slopes=None, points=200):
    """Shelf tolerances, before and after: the worst error as a share of the span (as report() and the
    filter-design doc's measured column), per Shape, position band and Q, over every Slope _gap_slopes
    gives, Gain +/-3 to +/-30 and Frequency 20 Hz to 20 kHz at 44.1, 48 and 96 kHz."""
    jobs = [(design, shape, fs, frequency, q, slopes or _gap_slopes(shape), points)
            for shape in shapes for design in GAP_DESIGNS for q in qs for fs in (44100, 48000, 96000)
            for frequency in (20, 200, 1000, 2000, 5000, 9000, 10000, 15000, 18000, 20000) if frequency <= 0.91 * fs / 2]
    worst = {}
    for design, shape, q, found in _parallel(_shelf_job, jobs, workers):
        for band, value in found.items():
            key = (shape, band, q, design)
            if value[0] > worst.get(key, (-1,))[0]:
                worst[key] = value
    print("shelf tolerances: worst error as a share of the span, over the fractional Slopes")
    print(f"{'shape':>10} {'band':>7} {'Q':>5}  {'before':>6} {'after':>6} {'limit':>5}   worst after (before)")
    for shape, band, q, design in sorted(worst):
        if design != "after":
            continue
        b, a = worst[(shape, band, q, "before")], worst[(shape, band, q, "after")]
        limit = SHELF_LIMITS[band][q > RESONANT_SHELF_Q]
        mark = " over" if a[0] > limit else ""
        print(f"{shape:>10} {band:>7} {q:>5g}  {b[0]:6.1%} {a[0]:6.1%} {limit:5.0%}{mark:5}"
              f" Slope={a[2]:g} fs={a[3]} F={a[4]} G={a[5]} (Slope={b[2]:g} fs={b[3]} F={b[4]} G={b[5]})")


def report_rescale(steps=(1, 2, 3, 4), gains=(-30, -18, -6, -1, -0.1, 0.1, 1, 6, 18, 30)):
    """The corner rescale by newton_corner against the lab's bisection, over every Slope 0.01 dB/oct apart
    (Shelves: every 0.07 dB/oct, at each of gains): the worst corner error in cents; and at that worst
    case, the largest difference it makes to the analog response, in dB, at Q 0.71 and at Q 40."""
    print("corner rescale: Newton steps in ln(frequency) from the whole-order corner, against bisection")
    print(f"{'shapes':>7} {'steps':>5}  {'cents':>8} {'dB, Q 0.71':>10} {'dB, Q 40':>9}   worst at")
    grid = [2 ** (i / 48) for i in range(-48 * 8, 48 * 8 + 1)]
    for kind, slopes, gs in (("cuts", [i / 100 for i in range(601, 9600)], (0.0,)),
                             ("shelves", [i / 100 for i in range(601, 9600, 7)], gains)):
        problems = []
        for slope in slopes:
            nu = slope / 6
            if nu == math.floor(nu):
                continue
            for gain in gs:
                sections, level = _corner_problem("low-shelf" if gain else "high-cut", nu, gain)
                sign = -1 if gain < 0 else 1  # the level falls through the corner, but a cut Shelf's rises
                problems.append((slope, gain, sections, level,
                                 _crossing(lambda x: sign * (_analog_db(sections, x, 1) - level))))
        for n in steps:
            cents, slope, gain, exact, c = max(
                (abs(1200 * math.log2(max(newton_corner(s, lv, n), 1e-30) / e)), sl, g, e, newton_corner(s, lv, n))
                for sl, g, s, lv, e in problems)
            effect = []
            for q in (math.sqrt(0.5), 40.0):
                if gain:
                    sections = _morph_low_shelf_unscaled(slope / 6, 10 ** (gain / 20), q)
                else:
                    sections = [a for a, _ in _morph_prototype(slope / 6, q)]
                levels = [(_analog_db(sections, x * exact, 1), _analog_db(sections, x * c, 1)) for x in grid] if c > 0 else []
                effect.append(max((abs(a - b) for a, b in levels if max(a, b) > -60), default=math.inf))
            print(f"{kind:>7} {n:>5}  {cents:8.2g} {effect[0]:10.2g} {effect[1]:9.2g}   Slope={slope:g}"
                  + (f" G={gain:g}" if gain else ""))


GAPS = ("seams", "shelves", "rescale")


def gap_options(parser):
    def gap(name):
        if name not in GAPS:
            raise argparse.ArgumentTypeError(f"choose from {', '.join(GAPS)}")
        return name
    parser.add_argument("gaps", nargs="*", type=gap, metavar="gap", help=f"{', '.join(GAPS)} (default: all)")
    parser.add_argument("--shapes", nargs="+", choices=FRACTIONAL_SHAPES, help="seams: default all; shelves: default low-shelf high-shelf")
    parser.add_argument("--q", type=float, nargs="+", default=[0.71, 2, 2.25, 2.5, 3, 4, 10, 20, 40], help="shelves")
    parser.add_argument("--slopes", type=float, nargs="+", help="shelves: default 8 between each pair of whole orders")
    parser.add_argument("--jobs", type=int, default=4, help="worker processes")


@command("slope-gaps", options=gap_options)
def run_slope_gaps(args):
    """ADR 0006's gaps, before (fractional_design) and after (continuous_design)."""
    gaps = args.gaps or GAPS
    if "seams" in gaps:
        report_seams(args.shapes or FRACTIONAL_SHAPES, args.jobs)
        print()
    if "shelves" in gaps:
        report_shelves(args.shapes or ("low-shelf", "high-shelf"), args.q, args.jobs, args.slopes)
        print()
    if "rescale" in gaps:
        report_rescale()


# --- Dynamics gain computer (engine/src/Dynamics.cpp) ---------------------------------------------

KNEE_DB = 3.0


def movement(overshoot_db, full_range_overshoot_db, knee_db=KNEE_DB):
    """A Dynamic Band's movement, 0 to 1 of its Dynamic Range, for a steady level overshoot_db above
    Threshold: a smoothstep from knee_db below Threshold to full_range_overshoot_db + knee_db above it."""
    x = min(max((overshoot_db + knee_db) / (full_range_overshoot_db + 2 * knee_db), 0.0), 1.0)
    return x * x * (3 - 2 * x)


@command("gain-computer",
         options=lambda parser: parser.add_argument("--overshoots", type=float, nargs="+", default=[12]))
def run_gain_computer(args):
    report_gain_computer(args.overshoots)


def report_gain_computer(full_range_overshoots, ranges=(6, 12, 18, 30), overshoots=(0, 3, 6, 9, 12, 15)):
    """Movement in dB at each overshoot above Threshold, per Dynamic Range, and the steepest slope
    (dB of movement per dB of level, at the curve's middle); a slope above 1 means a cut Band's
    output falls as its detection rises there."""
    for full in full_range_overshoots:
        print(f"full range at {full:g} dB + {KNEE_DB:g} dB knee above Threshold")
        print(f"{'range':>6}  " + " ".join(f"{o:>6g}" for o in overshoots) + f"  {'slope':>6}")
        for r in ranges:
            moved = " ".join(f"{r * movement(o, full):6.2f}" for o in overshoots)
            print(f"{r:>6g}  {moved}  {1.5 * r / (full + 2 * KNEE_DB):6.2f}")
        print()


# --- Auto Threshold (engine/src/Dynamics.cpp) -----------------------------------------------------
# A model of a Dynamic Bell's detector (1 kHz, Q 1, its region band-pass, 5 ms power) and gain computer
# with Auto Attack and Release, over the DynamicsTest material. Mirrors the Engine within a few hundredths.

DETECTOR_FS, RUN_LENGTH = 48000.0, 16


def _coefficient(seconds):
    return 1 - math.exp(-1 / (seconds * DETECTOR_FS))


def detection_levels(seconds, level_db, envelope_db, seed=7):
    """Detection levels in dB of white noise at level_db shaped by envelope_db(t), in a 1 kHz Q 1 region."""
    import random
    rnd = random.Random(seed)
    (s,), _ = band_pass(DETECTOR_FS, 1000.0, q=1.0, order=1)
    x1 = x2 = y1 = y2 = power = 0.0
    c = _coefficient(0.005)
    levels = []
    for n in range(int(seconds * DETECTOR_FS)):
        x = 10 ** ((level_db + envelope_db(n / DETECTOR_FS)) / 20) * rnd.gauss(0, 1)
        y = s.b0 * x + s.b1 * x1 + s.b2 * x2 - s.a1 * y1 - s.a2 * y2
        x2, x1, y2, y1 = x1, x, y1, y
        power += c * (y * y - power)
        levels.append(10 * math.log10(2 * power + 1e-30))
    return levels


def auto_threshold_movement(levels, spreads, rise, fall, hold=0.25, full=12.0, gate=-80.0):
    """Movement (0 to 1) after each run of a Band in Auto Threshold: mean + spreads x std of the gated
    levels, each rising over rise and falling over fall seconds (a plain mean until rise is heard)."""
    cr, cf, cs = _coefficient(rise), _coefficient(fall), _coefficient(0.5)
    mean = variance = heard = moved = sustain = 0.0
    out = []
    for r in range(0, len(levels) - RUN_LENGTH + 1, RUN_LENGTH):
        run = levels[r:r + RUN_LENGTH]
        for level in run:
            if level > gate:
                heard = min(heard + 1, 1 / cr)
                mean += max(cr if level > mean else cf, 1 / heard) * (level - mean)
                d2 = (level - mean) ** 2
                variance += max(cr if d2 > variance else cf, 1 / heard) * (d2 - variance)
        threshold = mean + spreads * math.sqrt(variance)
        attack = _coefficient(0.020 * KNEE_DB / (KNEE_DB + max(max(run) - threshold, 0)))
        release = _coefficient(0.040 + 0.460 * sustain)
        for level in run:
            target = movement(level - threshold, full) if heard >= hold * DETECTOR_FS else 0.0
            moved += (attack if target > moved else release) * (target - moved)
            sustain += cs * (target - sustain)
        out.append(moved)
    return out


def auto_threshold_options(parser):
    parser.add_argument("--spreads", type=float, nargs="+", default=[0.2])
    parser.add_argument("--falls", type=float, nargs="+", default=[1.25])


@command("auto-threshold", options=auto_threshold_options)
def run_auto_threshold(args):
    report_auto_threshold(args.spreads, args.falls)


def report_auto_threshold(spreads, falls, rise=4.5):
    """The Auto Threshold criteria in tests/engine/DynamicsTest.cpp: on noise swinging +/-6 dB at 4 Hz,
    the least movement at a loud half-cycle's most and the most at a quiet half-cycle's least, over
    2-5 s; after 1 s of +6 dB on steady noise, the movement reached. The trough is release-limited:
    lowering it lowers the peak."""
    swing = detection_levels(5.0, -36, lambda t: 6 * math.sin(2 * PI * 4 * t))
    swell = detection_levels(4.0, -24, lambda t: 6.0 if t >= 3.0 else 0.0)
    at = lambda t: int(t * DETECTOR_FS / RUN_LENGTH)
    print(f"{'k':>5} {'fall':>5}  {'peak':>5} {'trough':>6} {'swell':>5}")
    for k in spreads:
        for fall in falls:
            m = auto_threshold_movement(swing, k, rise, fall)
            starts = [2 + i / 4 for i in range(12)]
            peak = min(max(m[at(t):at(t + 0.125)]) for t in starts)
            trough = max(min(m[at(t + 0.125):at(t + 0.25)]) for t in starts)
            swelled = auto_threshold_movement(swell, k, rise, fall)[at(4.0) - 1]
            print(f"{k:5g} {fall:5g}  {peak:5.2f} {trough:6.2f} {swelled:5.2f}")


# --- Bell Slope candidates (#19, docs/adr/0007) ---------------------------------------------------
# A steep Bell as a band shelf: the band-pass transform p = Q (s + 1/s) of a low-shelf prototype of
# order Slope / 12 in p, whose Gain is Gain at p = 0 (Frequency), 0 dB at p = infinity (DC and
# infinity), and half the Gain in dB at p = 1 (the half-Gain points, 1/Q of Frequency apart). At
# 12 dB/oct every prototype is the one first-order shelf with those three values, so every candidate
# is today's Bell there.
#
#     python3 tools/filter-lab/filterlab.py bell-butterworth --bell-slopes 12 24 48 96 --q 0.1 0.71 2 10 40
#     python3 tools/filter-lab/filterlab.py bell-chebyshev --bell-slopes 12 24 48 96 --q 0.1 0.71 2 10 40

CHEBYSHEV_RIPPLE = 0.05  # Chebyshev band shelf: ripple across its top, as a share of |Gain|


def butterworth_shelf_roots(order, gain_db):
    """Zeros and poles in p of the Butterworth low shelf (Gain at p = 0, 0 dB at infinity, half the
    Gain at p = 1): the Low Shelf target's roots, zeros on a circle of radius g^(1/2N), poles on the
    reciprocal one."""
    rz = (10 ** (gain_db / 20)) ** (1 / (2 * order))
    angles = [PI / 2 + (2 * k + 1) * PI / (2 * order) for k in range(order)]
    return [rz * cmath.exp(1j * a) for a in angles], [cmath.exp(1j * a) / rz for a in angles]


def chebyshev_shelf_roots(order, gain_db, ripple_share=CHEBYSHEV_RIPPLE):
    """Zeros and poles in p of a Chebyshev type I low shelf, |H|^2 = (g^2 + e^2 T_N^2) / (1 + e^2 T_N^2),
    equiripple across its top by ripple_share x |Gain|, with Gain exactly at p = 0 (so at even orders,
    where T_N(0) = 1, the ripple peaks rise above Gain), rescaled so p = 1 is the half-Gain point."""
    big = 10 ** (gain_db / 20)
    ripple = 10 ** (ripple_share * gain_db / 20)
    if order % 2:  # T_N(0) = 0: the top's peak is Gain, its troughs a ripple below
        g, low = big, big / ripple
        e2 = (g * g - low * low) / (low * low - 1)
    else:  # T_N(0) = 1: Gain is a trough, the peaks a ripple above
        g = big * ripple
        e2 = (g * g - big * big) / (big * big - 1)
    if order == 1:
        e2 = big  # one first-order shelf: Gain at 0, half the Gain at 1, nothing to ripple
    t_half = max(1.0, math.sqrt((g * g - big) / (e2 * (big - 1))))
    scale = math.cosh(math.acosh(t_half) / order)

    def roots(e):
        v = math.asinh(1 / e) / order
        thetas = [(2 * k - 1) * PI / (2 * order) for k in range(1, order + 1)]
        return [complex(-math.sinh(v) * math.sin(t), math.cosh(v) * math.cos(t)) / scale for t in thetas]

    e = math.sqrt(e2)
    return roots(e / g), roots(e)


def _band_quadratics(root, q):
    """p = Q (s + 1/s) maps a root r in p to the roots of s^2 - (r / Q) s + 1. A real r gives one real
    quadratic; a complex r (with its conjugate) gives a lower and an upper one. Returns [(d1, d0)]."""
    if abs(root.imag) < 1e-12:
        return [(-root.real / q, 1.0)]
    return sorted(((-2 * x.real, abs(x) ** 2) for x in _reciprocal_roots(root / q)), key=lambda d: d[1])


def band_shelf_analog(zeros, poles, q):
    """The band-pass transform of a low shelf given by its roots in p: second-order sections in s, each
    pairing a zero quadratic with the pole quadratic from the same prototype root (lower with lower,
    upper with upper). Only roots in the upper half plane (and real ones) are passed through."""
    sections = []
    for z, p in zip(zeros, poles):
        if z.imag < -1e-12:
            continue
        for (n1, n0), (d1, d0) in zip(_band_quadratics(z, q), _band_quadratics(p, q)):
            sections.append(Analog(1, n1, n0, 1, d1, d0))
    return sections


BELL_HOLD = 0.95  # where a steep Bell's sections are held, as a share of Nyquist
BELL_HOLD_MATCH = 0.9  # a held section's match point, as a share of the hold


def _band_shelf_section(a: Analog, frequency, fs):
    """One second-order section of a steep Bell, matched at its poles' natural frequency. Above the hold
    its poles are placed at the hold with the Q that makes them as loud there, relative to DC, as the
    analog poles are (never higher), and its zeros fit the analog section at DC, BELL_HOLD_MATCH x the
    hold and Nyquist. Not continuous as the natural frequency crosses the hold (see ADR 0007)."""
    b, w0 = normalised(a)
    reference, hold = frequency * w0, BELL_HOLD * fs / 2
    if reference <= hold:
        return match_section(b, reference, fs)
    poles = Analog(0, 0, 1, 1, b.d1, 1)
    held_q = min(1 / b.d1, math.sqrt(poles.squared(hold / reference) / poles.squared(0)))
    match = BELL_HOLD_MATCH * hold
    squared = lambda f: b.squared(f / reference)
    return match_magnitudes(Analog(0, 0, 1, 1, 1 / held_q, 1), hold, fs, match,
                            squared(0), squared(match), squared(fs / 2))


def _band_shelf_bell(prototype, fs, frequency, gain, q, slope):
    """A steep Bell from a prototype (roots in p): designed as a boost and inverted for a cut, as the
    Bell is today. At 12 dB/oct it is today's bell() exactly; steeper, each section is designed by
    _band_shelf_section."""
    order = max(1, round(slope / 12))
    analog = band_shelf_analog(*prototype(order, abs(gain)), q)
    if order == 1:  # (s^2 + s A/Q + 1) / (s^2 + s/(A Q) + 1): bell()'s one section
        boost = [match_section(analog[0], frequency, fs)]
    else:
        boost = [_band_shelf_section(a, frequency, fs) for a in analog]
    sign = 1 if gain >= 0 else -1
    return ([c if gain >= 0 else inverse(c) for c in boost],
            lambda f: sign * _analog_db(analog, f, frequency))


def bell_butterworth(fs, frequency, gain, q, slope=12):
    """Candidate A: the band-pass transform of the Butterworth Low Shelf of order Slope / 12."""
    return _band_shelf_bell(butterworth_shelf_roots, fs, frequency, gain, q, slope)


def bell_chebyshev(fs, frequency, gain, q, slope=12):
    """Candidate B: the band-pass transform of a Chebyshev type I low shelf of order Slope / 12."""
    return _band_shelf_bell(chebyshev_shelf_roots, fs, frequency, gain, q, slope)


BELL_SLOPES = {"bell-butterworth": bell_butterworth, "bell-chebyshev": bell_chebyshev}


def _upper_half_gain(frequency, q):
    return frequency * (math.sqrt(1 + 1 / (4 * q * q)) + 1 / (2 * q))


def bell_slope_options(parser):
    parser.add_argument("--q", type=float, nargs="+", default=[0.71])
    parser.add_argument("--bell-slopes", type=float, nargs="+", default=[12, 24, 48, 96])


@command(*BELL_SLOPES, options=bell_slope_options)
def run_bell_slope(args):
    report_bell_slope(BELL_SLOPES[args.command], slopes=args.bell_slopes, qs=args.q)


def report_bell_slope(shape, slopes=(12, 24, 48, 96), sample_rates=(44100, 48000, 96000),
                      frequencies=(20, 200, 1000, 2000, 5000, 9000, 10000, 15000, 18000, 20000),
                      gains=(-30, -18, -12, -6, -3, 3, 12, 18, 30), qs=(0.71,), points=200):
    """Worst error, louder and quieter than the target (for a boost; a cut mirrors it, louder there
    being less cut), as a share of |Gain| (the Bell test's measure) and in dB: per (position band, Slope,
    Q) by where Frequency sits, as the Bell test bands it; then per (position band, Slope) by where the
    upper half-Gain point sits, which is where a steep Bell's skirt meets Nyquist. Last, each Slope's
    sections per Band and how far its target strays from today's Bell (should be 0 at 12 dB/oct)."""
    by_frequency, by_edge = {}, {}
    for fs in sample_rates:
        for frequency in frequencies:
            if frequency > 0.91 * fs / 2:
                continue
            for gain in gains:
                for q in qs:
                    upper = _upper_half_gain(frequency, q)
                    edge = ">0.91" if upper > 0.91 * fs / 2 else position_band(upper, fs)
                    for slope in slopes:
                        sections, target = shape(fs, frequency, gain, q, slope)
                        w = by_frequency.setdefault((position_band(frequency, fs), slope, q), [0, 0, 0, 0])
                        v = by_edge.setdefault((edge, slope), [0, 0, 0, 0])
                        for i in range(points + 1):
                            f = 10 * (fs / 2 / 10) ** (i / points)
                            error = (cascade_db(sections, f, fs) - target(f)) * (1 if gain > 0 else -1)
                            for x in (w, v):
                                x[0], x[1] = max(x[0], error / abs(gain)), max(x[1], -error / abs(gain))
                                x[2], x[3] = max(x[2], error), max(x[3], -error)
    print("by Frequency / Nyquist")
    print(f"{'band':>7} {'slope':>5} {'Q':>6}  {'louder':>6} {'quieter':>7}  {'dB':>5} {'dB':>5}")
    for (band, slope, q), (up, down, up_db, down_db) in sorted(by_frequency.items()):
        print(f"{band:>7} {slope:>5g} {q:>6}  {up:6.1%} {down:7.1%}  {up_db:5.2f} {down_db:5.2f}")
    print()
    print("by upper half-Gain point / Nyquist, every Q")
    print(f"{'band':>7} {'slope':>5}  {'louder':>6} {'quieter':>7}  {'dB':>5} {'dB':>5}")
    for (band, slope), (up, down, up_db, down_db) in sorted(by_edge.items()):
        print(f"{band:>7} {slope:>5g}  {up:6.1%} {down:7.1%}  {up_db:5.2f} {down_db:5.2f}")
    print()
    print(f"{'slope':>5}  {'sections':>8}  {'target vs Bell, dB':>18}")
    for slope in slopes:
        off = 0.0
        for q in qs:
            for gain in gains:
                sections, target = shape(48000, 1000, gain, q, slope)
                _, bell_target = bell(48000, 1000, gain, q)
                off = max(off, max(abs(target(f) - bell_target(f)) for f in (10 * 2 ** (i / 8) for i in range(90))))
        print(f"{slope:>5g}  {len(sections):>8}  {off:18.2e}")


# --- Steep Bell continuity near Nyquist (#162, docs/adr/0007) -------------------------------------
# ADR 0007's held section, as first accepted ("hold"), isn't continuous: as a section's natural
# frequency crosses the hold, its match point jumps from its natural frequency (the hold) to
# BELL_HOLD_MATCH x the hold. ADR 0007 takes "slide", which slides the match point instead. Each candidate
# here designs one section of the Butterworth band shelf; bell-continuity measures how far the response
# moves between two Frequencies a small step apart, across every section's crossing (and on a grid
# away from them, for scale), and bell-candidates measures each candidate against ADR 0007's bands.
#
#     python3 tools/filter-lab/filterlab.py bell-continuity --candidates hold slide
#     python3 tools/filter-lab/filterlab.py bell-candidates --candidates hold slide --q 0.1 0.71 2 10 40 --at-crossings

BELL_LIMITS = {"<=0.45": 0.10, "<=0.73": 0.20, "<=0.91": 0.35}  # today's Bell test, as a share of |Gain|
FREQUENCY_RANGE = (10.0, 30000.0)  # the Frequency parameter's range


def _held_poles(b: Analog, reference, hold):
    """ADR 0007's held poles: at the hold, with the Q that makes them as loud there, relative to DC,
    as the analog poles are (never higher). Returns (held poles, held Q, analog Q)."""
    poles = Analog(0, 0, 1, 1, b.d1, 1)
    held_q = min(1 / b.d1, math.sqrt(poles.squared(hold / reference) / poles.squared(0)))
    return Analog(0, 0, 1, 1, 1 / held_q, 1), held_q, 1 / b.d1


def _section_hold(b: Analog, reference, fs):
    """ADR 0007 as accepted (_band_shelf_section): match at the natural frequency below the hold, at
    BELL_HOLD_MATCH x the hold above it."""
    hold = BELL_HOLD * fs / 2
    if reference <= hold:
        return match_section(b, reference, fs)
    held, _, _ = _held_poles(b, reference, hold)
    match = BELL_HOLD_MATCH * hold
    squared = lambda f: b.squared(f / reference)
    return match_magnitudes(held, hold, fs, match, squared(0), squared(match), squared(fs / 2))


def _section_clamp(b: Analog, reference, fs):
    """As _section_hold, but every section's match point is clamped to BELL_HOLD_MATCH x the hold, held
    or not: the match point no longer jumps as the natural frequency crosses the hold, and the held poles
    (Q unchanged there) don't either."""
    hold = BELL_HOLD * fs / 2
    match = min(reference, BELL_HOLD_MATCH * hold)
    squared = lambda f: b.squared(f / reference)
    if reference <= hold:
        return match_section(b, reference, fs, match_hz=match)
    held, _, _ = _held_poles(b, reference, hold)
    return match_magnitudes(held, hold, fs, match, squared(0), squared(match), squared(fs / 2))


def _section_blend(b: Analog, reference, fs):
    """As _section_clamp, with Band Pass's blend of magnitudes (filter-design doc, "Above 24 dB/oct"):
    the zeros fit the analog section's magnitude where the Q isn't lowered and, as it is lowered, more
    of the held section's own: the analog section moved to the hold, its zeros' and poles' damping both
    scaled by held Q / analog Q, weighted by sqrt(1 - held Q / analog Q)."""
    hold = BELL_HOLD * fs / 2
    if reference <= hold:
        return _section_clamp(b, reference, fs)
    held, held_q, analog_q = _held_poles(b, reference, hold)
    own = Analog(1, b.n1 * analog_q / held_q, b.n0, 1, 1 / held_q, 1)
    blend = math.sqrt(1 - held_q / analog_q)
    squared = lambda f: b.squared(f / reference) ** (1 - blend) * own.squared(f / hold) ** blend
    match = BELL_HOLD_MATCH * hold
    return match_magnitudes(held, hold, fs, match, squared(0), squared(match), squared(fs / 2))


BELL_SLIDE = 2 ** (1 / 24)  # the match point slides to BELL_HOLD_MATCH x the hold as the natural frequency rises this far above it


def _slid_match(reference, hold):
    """The hold while the natural frequency is at the hold, sliding (in log frequency, smoothly) to
    BELL_HOLD_MATCH x the hold as it rises BELL_SLIDE above it."""
    x = _smoothstep(math.log(reference / hold) / math.log(BELL_SLIDE))
    return hold * BELL_HOLD_MATCH ** x


def _section_slide(b: Analog, reference, fs):
    """As _section_hold, but above the hold the match point slides from the hold to BELL_HOLD_MATCH x the
    hold (_slid_match), so it no longer jumps as the natural frequency crosses the hold."""
    hold = BELL_HOLD * fs / 2
    if reference <= hold:
        return match_section(b, reference, fs)
    held, _, _ = _held_poles(b, reference, hold)
    match = _slid_match(reference, hold)
    squared = lambda f: b.squared(f / reference)
    return match_magnitudes(held, hold, fs, match, squared(0), squared(match), squared(fs / 2))


def _section_slide_blend(b: Analog, reference, fs):
    """_section_slide's match point with _section_blend's blend of magnitudes."""
    hold = BELL_HOLD * fs / 2
    if reference <= hold:
        return match_section(b, reference, fs)
    held, held_q, analog_q = _held_poles(b, reference, hold)
    own = Analog(1, b.n1 * analog_q / held_q, b.n0, 1, 1 / held_q, 1)
    blend = math.sqrt(1 - held_q / analog_q)
    squared = lambda f: b.squared(f / reference) ** (1 - blend) * own.squared(f / hold) ** blend
    match = _slid_match(reference, hold)
    return match_magnitudes(held, hold, fs, match, squared(0), squared(match), squared(fs / 2))


BELL_SECTIONS = {"hold": _section_hold, "clamp": _section_clamp, "blend": _section_blend,
                 "slide": _section_slide, "slide-blend": _section_slide_blend}


def _butterworth_bell_analog(gain, q, slope):
    """A boost's analog sections (ADR 0007), each normalised to its poles' natural frequency, which is
    independent of Frequency: [(section, natural frequency / Frequency)]."""
    order = max(1, round(slope / 12))
    return order, [normalised(a) for a in band_shelf_analog(*butterworth_shelf_roots(order, abs(gain)), q)]


def steep_bell(fs, frequency, gain, q, slope=12, section=_section_hold):
    """ADR 0007's steep Bell with each section designed by section(normalised analog, natural frequency
    in Hz, fs). At 12 dB/oct it is today's bell() exactly. Designed as a boost and inverted for a cut."""
    order, analog = _butterworth_bell_analog(gain, q, slope)
    if order == 1:
        boost = [match_section(analog[0][0], frequency * analog[0][1], fs)]
    else:
        boost = [section(b, frequency * w0, fs) for b, w0 in analog]
    sign = 1 if gain >= 0 else -1
    raw = band_shelf_analog(*butterworth_shelf_roots(order, abs(gain)), q)
    return ([c if gain >= 0 else inverse(c) for c in boost],
            lambda f: sign * _analog_db(raw, f, frequency))


def bell_candidate_options(parser):
    parser.add_argument("--candidates", nargs="+", default=list(BELL_SECTIONS), choices=list(BELL_SECTIONS))
    parser.add_argument("--q", type=float, nargs="+", default=[0.025, 0.1, 0.71, 2, 10, 40])
    parser.add_argument("--gains", type=float, nargs="+", default=[-30, -12, -3, 3, 12, 30],
                        help="bell-continuity only; bell-candidates uses report_bell_slope's Gains")
    parser.add_argument("--bell-slopes", type=float, nargs="+", default=[12, 24, 36, 48, 60, 72, 84, 96])
    parser.add_argument("--cents", type=float, default=0.1, help="the Frequency step, in cents")
    parser.add_argument("--at-crossings", action="store_true",
                        help="bell-candidates: add Frequencies either side of each section's crossing of the hold")


@command("bell-continuity", options=bell_candidate_options)
def run_bell_continuity(args):
    for name in args.candidates:
        print(f"candidate {name}")
        report_bell_continuity(BELL_SECTIONS[name], slopes=args.bell_slopes, qs=args.q, gains=args.gains,
                               cents=args.cents)
        print()


def _response_db(sections, fs, points):
    return [cascade_db(sections, f, fs) for f in points]


def report_bell_continuity(section, slopes, qs, gains, sample_rates=(44100, 48000, 96000), cents=0.1,
                           points=200):
    """The largest change in the response, at any of points frequencies from 10 Hz to Nyquist (log
    spaced, plus 64 linear ones across the top octave), between Frequencies a step of cents apart:
    straddling each section's crossing of the hold within the Frequency range, and 8 more points as its
    natural frequency rises BELL_SLIDE above the hold (where _slid_match slides), and, for scale, at
    Frequencies an octave apart across the range. Per Slope, worst over sample rates, Q and
    Gain, with its case."""
    grid = [FREQUENCY_RANGE[0] * 2 ** k for k in range(int(math.log2(FREQUENCY_RANGE[1] / FREQUENCY_RANGE[0])) + 1)]
    half = 2 ** (cents / 2400)
    worst = {}
    for fs in sample_rates:
        hold = BELL_HOLD * fs / 2
        at = [10 * (fs / 2 / 10) ** (i / points) for i in range(points + 1)]
        at += [fs / 4 * (1 + i / 64) for i in range(64)]
        for slope in slopes:
            for q in qs:
                for gain in gains:
                    _, analog = _butterworth_bell_analog(gain, q, slope)
                    crossings = [hold / w0 * BELL_SLIDE ** (k / 8) for _, w0 in analog for k in range(9)]
                    crossings = [f for f in crossings if FREQUENCY_RANGE[0] <= f <= FREQUENCY_RANGE[1]]
                    for kind, frequencies in (("crossing", crossings), ("grid", grid)):
                        for frequency in frequencies:
                            below = _response_db(steep_bell(fs, frequency / half, gain, q, slope, section)[0], fs, at)
                            above = _response_db(steep_bell(fs, frequency * half, gain, q, slope, section)[0], fs, at)
                            change = max(abs(x - y) for x, y in zip(below, above))
                            key = (slope, kind if kind == "grid" or frequency <= fs / 2 else "above")
                            if change > worst.get(key, (-1,))[0]:
                                worst[key] = (change, fs, frequency, q, gain)
    print(f"step {cents:g} cents; dB, worst over sample rates, Q and Gain; near the hold with Frequency up to")
    print("Nyquist and above it (to the top of the Frequency range), and on the octave grid")
    print(f"{'slope':>5}  {'near':>6}  {'above':>6}  {'grid':>6}   worst near / above")
    for slope in slopes:
        cells, cases = [], []
        for kind in ("crossing", "above"):
            change, fs, frequency, q, gain = worst.get((slope, kind), (0.0, 0, 0, 0, 0))
            cells.append(change)
            cases.append(f"fs={fs} F={frequency:.0f} Q={q} G={gain}" if fs else "none")
        print(f"{slope:>5g}  {cells[0]:6.3f}  {cells[1]:6.3f}  {worst[(slope, 'grid')][0]:6.3f}   {' / '.join(cases)}")



@command("bell-candidates", options=bell_candidate_options)
def run_bell_candidates(args):
    for name in args.candidates:
        print(f"candidate {name}")
        report_bell_candidate(BELL_SECTIONS[name], slopes=[s for s in args.bell_slopes if s > 12], qs=args.q,
                              crossings=args.at_crossings, cents=args.cents)
        print()


def report_bell_candidate(section, slopes, qs, sample_rates=(44100, 48000, 96000),
                          frequencies=(20, 200, 1000, 2000, 5000, 9000, 10000, 15000, 18000, 20000),
                          gains=(-30, -18, -12, -6, -3, 3, 12, 18, 30), points=200, crossings=False, cents=0.1):
    """report_bell_slope's measure, over every Slope given at once: the worst error louder and quieter
    than the target (for a boost; a cut mirrors it) as a share of |Gain|, banded by where the upper
    half-Gain point sits (ADR 0007), against today's Bell limits, with the worst case. With crossings,
    the grid adds the Frequencies a step of cents either side of each section's crossing of the hold
    (up to 0.91 x Nyquist), where a section sits at the hold and the fixed grid would miss it."""
    worst = {}
    for fs in sample_rates:
        hold = BELL_HOLD * fs / 2
        for gain in gains:
            for q in qs:
                for slope in slopes:
                    _, analog = _butterworth_bell_analog(gain, q, slope)
                    at = list(frequencies)
                    if crossings:
                        at += [hold / w0 * 2 ** (side * cents / 1200) for _, w0 in analog for side in (-1, 1)]
                    for frequency in at:
                        if not 20 <= frequency <= 0.91 * fs / 2:
                            continue
                        upper = _upper_half_gain(frequency, q)
                        edge = ">0.91" if upper > 0.91 * fs / 2 else position_band(upper, fs)
                        sections, target = steep_bell(fs, frequency, gain, q, slope, section)
                        for i in range(points + 1):
                            f = 10 * (fs / 2 / 10) ** (i / points)
                            error = (cascade_db(sections, f, fs) - target(f)) * (1 if gain > 0 else -1) / abs(gain)
                            for side, e in ((0, error), (1, -error)):
                                if e > worst.get((edge, side), (-1,))[0]:
                                    worst[(edge, side)] = (e, fs, frequency, q, gain, slope)
    print(f"by upper half-Gain point / Nyquist, Slopes {', '.join(f'{s:g}' for s in slopes)}")
    print(f"{'band':>7}  {'louder':>6} {'quieter':>7}  {'limit':>5}   worst case")
    for band in ("<=0.45", "<=0.73", "<=0.91", ">0.91"):
        if (band, 0) not in worst:
            continue
        up, down = worst[(band, 0)], worst[(band, 1)]
        limit = f"{BELL_LIMITS[band]:5.0%}" if band in BELL_LIMITS else "    -"
        e, fs, frequency, q, gain, slope = max(up, down)
        print(f"{band:>7}  {up[0]:6.1%} {down[0]:7.1%}  {limit}   fs={fs} F={frequency:.0f} Q={q} G={gain} slope={slope:g}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    commands = parser.add_subparsers(dest="command", required=True, metavar="command",
                                     help=", ".join(COMMANDS))
    for name, (options, _) in COMMANDS.items():
        options(commands.add_parser(name))
    args = parser.parse_args()
    COMMANDS[args.command][1](args)
