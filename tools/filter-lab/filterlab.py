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

To try a new design, write a function returning (digital sections, analog target in dB) like the
ones under "Shapes", and pass it to report().
"""

import argparse
import cmath
import math
from dataclasses import dataclass

PI = math.pi


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


def _morph_low_shelf_analog(nu, dc_gain, q):
    """Low Shelf of fractional order: each prototype pole r gives a zero at r x rz^w and a pole at r / rz^w,
    w its share (1 for a whole root), with rz^(2 sum w) = dc_gain, so a root coming in from infinity
    brings its step in gradually. Rescaled so it reaches half its Gain in dB at s = 1, at Q 0.71."""
    def build(q_):
        out, shares = [], []
        for a, share in _morph_prototype(nu, q_):
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

    sections = build(q)
    if nu == math.floor(nu) or dc_gain == 1:
        return sections
    plain, half = build(math.sqrt(0.5)), 10 * math.log10(dc_gain)
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
    which is the partial target). A Cut or Band Pass below 6 dB/oct grows in from passing unchanged."""
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
        return fractional_design(shape, fs, frequency, gain, q, slope)[1]
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
    morphing cascade, and its target the candidate."""
    def design(fs, frequency, gain, q, slope):
        sections, _ = fractional_design(shape, fs, frequency, gain, q, slope)
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


# --- Dynamics gain computer (engine/src/Dynamics.cpp) ---------------------------------------------

KNEE_DB = 3.0


def movement(overshoot_db, full_range_overshoot_db, knee_db=KNEE_DB):
    """A Dynamic Band's movement, 0 to 1 of its Dynamic Range, for a steady level overshoot_db above
    Threshold: a smoothstep from knee_db below Threshold to full_range_overshoot_db + knee_db above it."""
    x = min(max((overshoot_db + knee_db) / (full_range_overshoot_db + 2 * knee_db), 0.0), 1.0)
    return x * x * (3 - 2 * x)


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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("shape", choices=[*SHAPES, "gain-computer", "auto-threshold", "slopes"])
    parser.add_argument("--q", type=float, nargs="+", default=[0.71])
    parser.add_argument("--orders", type=int, nargs="+", default=[2])
    parser.add_argument("--overshoots", type=float, nargs="+", default=[12])
    parser.add_argument("--spreads", type=float, nargs="+", default=[0.2])
    parser.add_argument("--falls", type=float, nargs="+", default=[1.25])
    parser.add_argument("--slopes", type=float, nargs="+",
                        help="fractional Slopes in dB/oct (#18): the morphing cascade instead of --orders")
    parser.add_argument("--target", choices=TARGETS, default="partial", help="the candidate target for --slopes")
    args = parser.parse_args()
    if args.shape == "slopes":
        report_slopes()
        raise SystemExit
    if args.slopes:
        if args.shape not in FRACTIONAL_SHAPES:
            parser.error(f"--slopes works on {', '.join(FRACTIONAL_SHAPES)}")
        reporter = report_cut if args.shape in CUTS else report
        reporter(fractional_shape(args.shape, args.target), qs=args.q, orders=args.slopes)
        raise SystemExit
    if args.shape == "auto-threshold":
        report_auto_threshold(args.spreads, args.falls)
        raise SystemExit
    if args.shape == "gain-computer":
        report_gain_computer(args.overshoots)
        raise SystemExit
    reporter = report_phase if args.shape == "all-pass" else report_cut if args.shape in CUTS else report
    reporter(SHAPES[args.shape], qs=args.q, orders=args.orders)
