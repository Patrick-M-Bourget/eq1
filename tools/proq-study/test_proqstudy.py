"""Tests for proqstudy.py on a hand-written fixture (not a Pro-Q 4 preset).

    python3 -m unittest discover -s tools/proq-study
"""

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

import proqstudy

FIXTURE = """[Preset]
Signature=FQ4p
Version=4
Description="Hand-written fixture"
Tags=Test

[Parameters]
Band 1 Used=1
Band 1 Enabled=1
Band 1 Frequency=9.96578407287598
Band 1 Gain=-3.5
Band 1 Q=0.5
Band 1 Shape=0
Band 1 Slope=2
Band 1 Stereo Placement=2
Band 2 Used=1
Band 2 Enabled=0
Band 2 Frequency=6.64385618977472
Band 2 Gain=0
Band 2 Q=0.5
Band 2 Shape=2
Band 2 Slope=3
Band 2 Stereo Placement=0
Band 3 Used=0
Band 3 Shape=4
Band 4 Used=1
Band 4 Enabled=1
Band 4 Frequency=14
Band 4 Shape=4
Band 4 Slope=1.5
Band 4 Stereo Placement=4
Some Future Key=7
"""


def write(folder, relative, text):
    path = Path(folder) / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


class ConversionTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.preset = proqstudy.read_preset(write(self.folder.name, "Vocals/Fixture.ffp", FIXTURE), self.folder.name)

    def tearDown(self):
        self.folder.cleanup()

    def test_only_bands_in_use_are_kept(self):
        self.assertEqual([band["slot"] for band in self.preset["bands"]], [1, 2, 4])

    def test_category_is_the_folder(self):
        self.assertEqual(self.preset["category"], "Vocals")
        self.assertEqual(self.preset["name"], "Fixture")

    def test_frequency_is_log2_of_hz(self):
        self.assertAlmostEqual(self.preset["bands"][0]["frequency"], 1000.0, places=2)
        self.assertAlmostEqual(self.preset["bands"][1]["frequency"], 100.0, places=2)
        self.assertAlmostEqual(self.preset["bands"][2]["frequency"], 16384.0, places=1)

    def test_shape_follows_eq1s_order(self):
        self.assertEqual([band["shape"] for band in self.preset["bands"]], ["Bell", "Low Cut", "High Cut"])

    def test_gain_is_db_and_q_is_log_normalised(self):
        bell = self.preset["bands"][0]
        self.assertEqual(bell["gain"], -3.5)
        self.assertAlmostEqual(bell["q"], 1.0)  # 0.5 is the middle of 0.025 to 40 in log space

    def test_disabled_band_is_bypassed(self):
        self.assertEqual([band["bypass"] for band in self.preset["bands"]], [False, True, False])

    def test_slope_points_and_fractions(self):
        self.assertEqual(self.preset["bands"][1]["slope"], 24.0)  # point 3
        self.assertEqual(self.preset["bands"][2]["slope"], 15.0)  # halfway between 12 and 18

    def test_stereo_placement(self):
        self.assertEqual([band["placement"] for band in self.preset["bands"]], ["Stereo", "Left", "Side"])


class SlopeTest(unittest.TestCase):
    def test_points(self):
        self.assertEqual([proqstudy.slope(i) for i in range(9)], [6, 12, 18, 24, 30, 36, 48, 72, 96])

    def test_fractions_interpolate_linearly_between_points(self):
        self.assertEqual(proqstudy.slope(0.5), 9.0)
        self.assertEqual(proqstudy.slope(6.5), 60.0)
        self.assertEqual(proqstudy.slope(7.25), 78.0)

    def test_brickwall_is_the_last_point(self):
        self.assertEqual(proqstudy.slope(9), "Brickwall")


DYNAMIC_FIXTURE = """[Parameters]
Band 1 Used=1
Band 1 Shape=0
Band 1 Dynamic Range=-6
Band 1 Dynamics Enabled=1
Band 1 Dynamics Auto=0
Band 1 Threshold=1
Band 1 Attack=30
Band 1 Release=70
Band 1 External Side Chain=1
Band 1 Side Chain Filtering=1
Band 1 Side Chain Low Frequency=6.64385618977472
Band 1 Side Chain High Frequency=12.2877123795494
Band 1 Spectral Enabled=1
Band 2 Used=1
Band 2 Shape=3
Band 2 Dynamic Range=4
Band 2 Dynamics Enabled=0
Band 2 Threshold=0.5
Band 2 Speakers=3
Band 3 Used=1
Band 3 Shape=6
Band 3 Slope=9
Band 4 Used=1
Band 4 Shape=2
Band 4 Dynamic Range=-6
Processing Mode=2
Character=1
"""


class DynamicsTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.preset = proqstudy.read_preset(write(self.folder.name, "Dynamic.ffp", DYNAMIC_FIXTURE), self.folder.name)
        self.bands = self.preset["bands"]

    def tearDown(self):
        self.folder.cleanup()

    def test_top_level_preset_has_no_category_folder(self):
        self.assertEqual(self.preset["category"], "(top level)")

    def test_a_dynamic_band_needs_a_shape_with_gain_and_a_dynamic_range(self):
        self.assertEqual([band["dynamic"] for band in self.bands], [True, True, False, False])
        self.assertEqual(self.bands[0]["dynamicRange"], -6.0)

    def test_threshold_at_the_top_is_auto(self):
        self.assertTrue(self.bands[0]["thresholdAuto"])
        self.assertFalse(self.bands[1]["thresholdAuto"])
        self.assertAlmostEqual(self.bands[1]["threshold"], -30.0)

    def test_attack_release_and_dynamics_bypass(self):
        self.assertEqual((self.bands[0]["attack"], self.bands[0]["release"]), (30.0, 70.0))
        self.assertEqual([band["dynamicsBypass"] for band in self.bands[:2]], [False, True])

    def test_detection(self):
        first, second = self.bands[0], self.bands[1]
        self.assertEqual((first["detectionSource"], first["detectionRange"]), ("External", "Free"))
        self.assertAlmostEqual(first["detectionLow"], 100.0, places=2)
        self.assertAlmostEqual(first["detectionHigh"], 5000.0, places=1)
        self.assertEqual((second["detectionSource"], second["detectionRange"]), ("Internal", "Band"))

    def test_flags_keep_the_band(self):
        self.assertEqual(len(self.bands), 4)
        self.assertEqual(self.bands[0]["flags"], ["spectral dynamics"])
        self.assertEqual(self.bands[1]["flags"], ["surround Speakers"])
        self.assertEqual(self.bands[2]["flags"], ["Brickwall on Band Pass"])
        self.assertEqual(self.preset["flags"], ["Processing Mode Linear Phase", "Character Subtle"])
        self.assertTrue(proqstudy.is_flagged(self.preset))


class CommandLineTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.source = Path(self.folder.name) / "presets"
        self.out = Path(self.folder.name) / "out"
        write(self.source, "Vocals/Fixture.ffp", FIXTURE)
        write(self.source, "Vocals/Dynamic.ffp", DYNAMIC_FIXTURE)
        write(self.source, "Drums/Notes.txt", "not a preset")

    def tearDown(self):
        self.folder.cleanup()

    def run_main(self, *args):
        with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err:
            code = proqstudy.main(list(args))
        return code, out.getvalue(), err.getvalue()

    def test_missing_source_is_a_clear_error(self):
        code, _, err = self.run_main("corpus", "--source", str(self.source / "nowhere"), "--out", str(self.out))
        self.assertNotEqual(code, 0)
        self.assertIn("nowhere", err)
        self.assertFalse(self.out.exists())

    def test_corpus_writes_every_preset_and_counts_the_flagged(self):
        code, out, _ = self.run_main("corpus", "--source", str(self.source), "--out", str(self.out))
        self.assertEqual(code, 0)
        self.assertIn("2 presets, 1 with flagged features", out)
        corpus = json.loads((self.out / "corpus.json").read_text())
        self.assertEqual(sorted(preset["name"] for preset in corpus), ["Dynamic", "Fixture"])
        self.assertIn("Vocals / Fixture", (self.out / "corpus.txt").read_text())

    def test_stats_writes_a_report_per_category(self):
        code, _, _ = self.run_main("stats", "--source", str(self.source), "--out", str(self.out))
        self.assertEqual(code, 0)
        report = (self.out / "stats.md").read_text()
        self.assertIn("## Vocals (2 presets)", report)
        self.assertIn("Bands per preset: 3 x1, 4 x1", report)
        self.assertIn("Stereo Placement: Stereo 5, Left 1, Side 1", report)
        self.assertIn("Dynamic Bands: 2 of 7 Bands, in 1 of 2 presets; 1 cut, 1 boost", report)


class StatsTest(unittest.TestCase):
    def test_frequencies_cluster_by_octave(self):
        self.assertEqual(proqstudy.octave_clusters([100.0, 120.0, 1000.0, 1300.0, 2700.0]),
                         [(125.0, 2), (1000.0, 2), (2000.0, 1)])

    def test_hz_reads_as_people_write_it(self):
        self.assertEqual([proqstudy.hz(f) for f in (31.5, 999.9, 1250.0)], ["31.5 Hz", "1 kHz", "1.25 kHz"])
