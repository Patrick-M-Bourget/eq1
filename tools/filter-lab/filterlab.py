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
    """The DynamicsTest criteria (#152): on noise swinging +/-6 dB at 4 Hz, the least movement at a loud
    half-cycle's most and the most at a quiet half-cycle's least, over 2-5 s; after 1 s of +6 dB on
    steady noise, the movement reached. The trough is release-limited: lowering it lowers the peak."""
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
    parser.add_argument("shape", choices=[*SHAPES, "gain-computer", "auto-threshold"])
    parser.add_argument("--q", type=float, nargs="+", default=[0.71])
    parser.add_argument("--orders", type=int, nargs="+", default=[2])
    parser.add_argument("--overshoots", type=float, nargs="+", default=[12])
    parser.add_argument("--spreads", type=float, nargs="+", default=[0.2])
    parser.add_argument("--falls", type=float, nargs="+", default=[1.25])
    args = parser.parse_args()
    if args.shape == "auto-threshold":
        report_auto_threshold(args.spreads, args.falls)
        raise SystemExit
    if args.shape == "gain-computer":
        report_gain_computer(args.overshoots)
        raise SystemExit
    reporter = report_phase if args.shape == "all-pass" else report_cut if args.shape in CUTS else report
    reporter(SHAPES[args.shape], qs=args.q, orders=args.orders)
