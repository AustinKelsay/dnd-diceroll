import array
import copy
import json
import math
from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import audio_beds

ROOT = Path(__file__).resolve().parents[2]
AMBIENCE = ROOT / "Art/audio-ambience/r001"
MUSIC = ROOT / "Art/audio-music/r001"


def declared(header, enum):
    """An enum's values (minus None), read from the header the game compiles."""
    text = (ROOT / "Source/DnDDiceRoll/Public/Presentation" / header).read_text()
    body = re.search(r"enum class " + enum + r" : uint8\s*\{(.*?)\};", text, re.S).group(1)
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", body, flags=re.S)
    return [name.strip() for name in body.split(",") if name.strip() and name.strip() != "None"]


def sine(frames, cycles, channels=1, scale=20000):
    """A whole number of cycles over the buffer, so it wraps without a jump."""
    out = array.array("h")
    for frame in range(frames):
        value = int(round(scale * math.sin(2 * math.pi * cycles * frame / frames)))
        out.extend([value] * channels)
    return out


class AudioBedToolTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads((AMBIENCE / "recipe.json").read_text())

    def test_the_shipped_recipe_is_valid_and_matches_the_game(self):
        self.assertEqual(audio_beds.validate_recipe(self.recipe, declared("ExpeditionAmbience.h", "EExpeditionAmbience")), [])

    def test_every_bed_is_a_cc0_checksummed_loop_with_a_rendered_wav(self):
        for name, source in self.recipe["sources"].items():
            self.assertIn("CC0", source["license"], name)
            for digest in source["files"].values():
                self.assertRegex(digest, r"^[0-9a-f]{64}$")
        for name, plan in self.recipe["pieces"].items():
            self.assertTrue(plan["loop"], name)
            self.assertTrue((AMBIENCE / self.recipe["output"] / f"{name}.wav").is_file(), name)

    def test_ambience_sits_under_every_effect_category(self):
        cues = json.loads((ROOT / "Art/audio-sources/r001/recipe.json").read_text())
        quietest = min(category["lufs"] for category in cues["categories"].values())
        self.assertLess(self.recipe["categories"]["ambience"]["lufs"], quietest)

    def test_refuses_broken_recipes(self):
        broken = copy.deepcopy(self.recipe)
        bed = broken["pieces"]["Vigil"]
        bed["crossfade"] = bed["length"]
        bed["layers"][0]["pan"] = 2
        bed["layers"][0]["repeat"] = 0
        bed["layers"][1]["wobble"] = True
        broken["pieces"]["Extra"] = copy.deepcopy(bed)
        problems = "\n".join(audio_beds.validate_recipe(broken, ["WarTable", "Vigil", "Root", "Vault"]))
        for expected in ("crossfade", "pan must", "repeat must", "unknown layer keys", "Extra: not declared", "order must"):
            self.assertIn(expected, problems)

    def test_a_layer_can_sound_at_several_times(self):
        self.assertEqual(audio_beds.placements({"at": [1.0, 2.5]}), [1.0, 2.5])
        self.assertEqual(audio_beds.placements({"at": 3.0}), [3.0])
        self.assertEqual(audio_beds.placements({}), [0.0])

    def test_pan_is_constant_power(self):
        for pan in (-1.0, -0.4, 0.0, 0.7, 1.0):
            left, right = audio_beds.pan_gains(pan)
            self.assertAlmostEqual(left * left + right * right, 1.0)
        self.assertAlmostEqual(audio_beds.pan_gains(-1.0)[1], 0.0)
        self.assertIn("pan=stereo|c0=1.0000*c0|c1=1.0000*c0", audio_beds.channel_map(1, {}))
        self.assertIn("0.5000*c0+0.5000*c1", audio_beds.channel_map(2, {"mono": True}))

    def test_folding_makes_the_end_run_into_the_start(self):
        # A ramp rendered past the loop length: after folding, the first frame is the frame that followed the last one.
        length, fade = 1000, 100
        render = array.array("f", [float(i) for i in range(length + fade)])
        loop = audio_beds.fold_loop(render, 1, length, fade)
        self.assertEqual(len(loop), length)
        self.assertAlmostEqual(loop[0], render[length])
        self.assertEqual(loop[length - 1], render[length - 1])
        self.assertAlmostEqual(loop[fade - 1], render[fade - 1] * math.sin((fade - 1) / fade * math.pi / 2) +
                               render[length + fade - 1] * math.cos((fade - 1) / fade * math.pi / 2), places=3)
        with self.assertRaises(ValueError):
            audio_beds.fold_loop(render, 1, length, fade + 1)

    def test_seam_report_passes_a_clean_wrap_and_fails_a_click(self):
        clean = audio_beds.seam_report(sine(4800, 30, channels=2), 2, 48000)
        self.assertTrue(clean["ok"], clean)
        self.assertLessEqual(clean["ratio"], 1.0)
        clicked = sine(4800, 30)
        clicked[-1] = 20000
        report = audio_beds.seam_report(clicked, 1, 48000)
        self.assertFalse(report["ok"])
        self.assertGreater(report["ratio"], 1.0)

    def test_seam_report_fails_a_level_step(self):
        quiet_end = sine(48000, 2000)
        for index in range(len(quiet_end) - 2400, len(quiet_end)):
            quiet_end[index] //= 4
        report = audio_beds.seam_report(quiet_end, 1, 48000)
        self.assertGreater(report["level_db"], audio_beds.SEAM_LEVEL_DB)
        self.assertFalse(report["ok"])

    def test_the_rendered_beds_loop_cleanly(self):
        for name in self.recipe["order"]:
            path = AMBIENCE / self.recipe["output"] / f"{name}.wav"
            if path.read_bytes()[:40].startswith(b"version https://git-lfs"):
                self.skipTest("LFS pointers only (CI checks out without LFS); measured on a full checkout")
            samples, channels, rate = audio_beds.read_wav(path)
            self.assertEqual((channels, rate), (2, 48000), name)
            self.assertAlmostEqual(len(samples) / channels / rate, self.recipe["pieces"][name]["length"], places=2)
            self.assertTrue(audio_beds.seam_report(samples, channels, rate)["ok"], name)


class MusicRecipeTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads((MUSIC / "recipe.json").read_text())

    def test_the_music_recipe_is_valid_and_matches_the_game(self):
        names = declared("ExpeditionMusic.h", "EExpeditionMusic") + declared("ExpeditionMusic.h", "EExpeditionSting")
        self.assertEqual(audio_beds.validate_recipe(self.recipe, names), [])

    def test_tracks_loop_and_stings_play_once(self):
        stings = declared("ExpeditionMusic.h", "EExpeditionSting")
        for name, plan in self.recipe["pieces"].items():
            self.assertEqual(bool(plan.get("loop")), name not in stings, name)
            self.assertTrue((MUSIC / self.recipe["output"] / f"{name}.wav").is_file(), name)
        for name, source in self.recipe["sources"].items():
            self.assertIn("CC0", source["license"], name)

    def test_music_sits_below_effects_and_above_ambience(self):
        cues = json.loads((ROOT / "Art/audio-sources/r001/recipe.json").read_text())
        ambience = json.loads((AMBIENCE / "recipe.json").read_text())
        effects = [category["lufs"] for category in cues["categories"].values()]
        music = self.recipe["categories"]["music"]["lufs"]
        self.assertLessEqual(music, min(effects))
        self.assertGreater(music, ambience["categories"]["ambience"]["lufs"])
        self.assertLess(self.recipe["categories"]["sting"]["lufs"], max(effects))

    def test_the_level_rule_is_off_only_for_music(self):
        self.assertIsNone(self.recipe["categories"]["music"]["seam_level_db"])
        loud_start = sine(48000, 2000)
        for index in range(len(loud_start) - 2400, len(loud_start)):
            loud_start[index] //= 4
        self.assertFalse(audio_beds.seam_report(loud_start, 1, 48000)["ok"])
        self.assertTrue(audio_beds.seam_report(loud_start, 1, 48000, None)["ok"])

    def test_the_rendered_tracks_loop_cleanly(self):
        for name, plan in self.recipe["pieces"].items():
            path = MUSIC / self.recipe["output"] / f"{name}.wav"
            if path.read_bytes()[:40].startswith(b"version https://git-lfs"):
                self.skipTest("LFS pointers only (CI checks out without LFS); measured on a full checkout")
            samples, channels, rate = audio_beds.read_wav(path)
            self.assertEqual((channels, rate), (2, 48000), name)
            self.assertAlmostEqual(len(samples) / channels / rate, plan["length"], places=2)
            if plan.get("loop"):
                self.assertTrue(audio_beds.seam_report(samples, channels, rate, None)["ok"], name)


if __name__ == "__main__":
    unittest.main()
