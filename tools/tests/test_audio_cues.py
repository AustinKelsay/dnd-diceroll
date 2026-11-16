import copy
import json
import math
from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import audio_cues

ROOT = Path(__file__).resolve().parents[2]
SUMMARY = """[Parsed_ebur128_1 @ 0x5] Summary:

  Integrated loudness:
    I:         -18.4 LUFS
    Threshold: -28.6 LUFS

  Loudness range:
    LRA:         0.0 LU

  True peak:
    Peak:       -1.3 dBFS
"""


def declared_cues():
    """The EExpeditionCue values, read from the header the game compiles."""
    header = (ROOT / "Source/DnDDiceRoll/Public/Presentation/ExpeditionCue.h").read_text()
    body = re.search(r"enum class EExpeditionCue : uint8\s*\{(.*?)\};", header, re.S).group(1)
    return [name.strip() for name in body.replace("\n", " ").split(",") if name.strip()]


class AudioCueToolTests(unittest.TestCase):
    def test_parses_the_ebur128_summary(self):
        self.assertEqual(audio_cues.parse_ebur128("frame lines\n" + SUMMARY), (-18.4, -1.3))

    def test_silence_parses_as_minus_infinity(self):
        silent = SUMMARY.replace("-18.4 LUFS", "-70.0 LUFS").replace("-1.3 dBFS", "-inf dBFS")
        loudness, peak = audio_cues.parse_ebur128(silent)
        self.assertEqual(loudness, -70.0)
        self.assertEqual(peak, -math.inf)

    def test_missing_summary_is_an_error(self):
        with self.assertRaises(ValueError):
            audio_cues.parse_ebur128("no summary here")

    def test_gain_reaches_the_target_when_the_peak_allows(self):
        self.assertAlmostEqual(audio_cues.normalising_gain(-24.0, -10.0, -19.0, -1.0), 5.0)

    def test_gain_stops_at_the_true_peak_ceiling(self):
        self.assertAlmostEqual(audio_cues.normalising_gain(-30.0, -4.0, -19.0, -1.0), 3.0)

    def test_gain_can_lower_a_loud_mix(self):
        self.assertAlmostEqual(audio_cues.normalising_gain(-12.0, -0.5, -19.0, -1.0), -7.0)

    def test_silent_mix_is_refused(self):
        with self.assertRaises(ValueError):
            audio_cues.normalising_gain(-70.0, -math.inf, -19.0, -1.0)

    def test_limiter_is_skipped_without_headroom_and_bounded_otherwise(self):
        self.assertIsNone(audio_cues.limiter_filter(-6.0, 0))
        chain = audio_cues.limiter_filter(-6.0, 8.0)
        self.assertIn("volume=6.000dB", chain)
        self.assertIn("limit=0.39811", chain)
        self.assertIn("level=false", chain)
        # The filter cannot limit by more than 24 dB.
        self.assertIn("limit=0.06250", audio_cues.limiter_filter(0.0, 40.0))


class AudioCueRecipeTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads(audio_cues.RECIPE.read_text())

    def test_recipe_covers_exactly_the_declared_cues(self):
        cues = declared_cues()
        self.assertEqual(self.recipe["cue_order"], cues)
        self.assertEqual(audio_cues.validate_recipe(self.recipe, cues), [])

    def test_repeating_cues_have_two_to_four_variations(self):
        for cue in ("Hit", "Step", "Swing", "DiceRoll", "DiceLand", "Click"):
            self.assertIn(len(self.recipe["cues"][cue]["variations"]), range(2, 5), cue)

    def test_every_source_is_cc0_and_traceable(self):
        for name, source in self.recipe["sources"].items():
            self.assertIn("CC0", source["license"], name)
            self.assertTrue(source["url"].startswith("https://"), name)
            self.assertTrue(source["author"].strip(), name)
            self.assertTrue(source["files"], name)
            for digest in source["files"].values():
                self.assertRegex(digest, r"^[0-9a-f]{64}$")

    def test_every_variation_has_its_rendered_file(self):
        folder = audio_cues.RECIPE.parent / "cues"
        for cue, plan in self.recipe["cues"].items():
            for index in range(len(plan["variations"])):
                self.assertTrue((folder / audio_cues.output_name(cue, index)).is_file(), (cue, index))

    def test_validation_reports_problems(self):
        broken = copy.deepcopy(self.recipe)
        del broken["cues"]["Hit"]
        broken["cues"]["Swing"]["variations"] *= 2
        broken["cues"]["Step"]["category"] = "nope"
        broken["cues"]["Bow"]["variations"][0]["layers"][0]["typo"] = 1
        broken["cues"]["Bow"]["variations"][1]["layers"][0]["file"] = "not/checksummed.wav"
        problems = "\n".join(audio_cues.validate_recipe(broken, declared_cues()))
        for expected in ("Hit: no recipe", "Swing: 8 variations", "Step: unknown category", "unknown layer keys ['typo']", "has no checksum"):
            self.assertIn(expected, problems)


if __name__ == "__main__":
    unittest.main()
