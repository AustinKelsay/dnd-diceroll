"""The journey map's positions are painted on SM_Journey_Map, so the Blender source (journey_layout.json) and the game (JourneyLayout.h) must agree (#248)."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "Source" / "DnDDiceRoll" / "Public" / "Presentation" / "JourneyLayout.h"
LAYOUT = ROOT / "Art" / "dark-fantasy-sources" / "r001-journey" / "journey_layout.json"


def vectors(text):
    return [(float(x), float(y)) for x, y in re.findall(r"FVector2D\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)", text)]


class JourneyLayoutTests(unittest.TestCase):
    def setUp(self):
        self.header = HEADER.read_text()
        self.layout = json.loads(LAYOUT.read_text())

    def function(self, name):
        """The body of `inline ... Name(...) { ... }`, by brace matching."""
        match = re.search(r"inline [\w<>]+ " + name + r"\([^)]*\)\s*\{", self.header)
        self.assertIsNotNone(match, name)
        depth, start = 1, match.end()
        for index in range(start, len(self.header)):
            depth += {"{": 1, "}": -1}.get(self.header[index], 0)
            if depth == 0:
                return self.header[start:index]
        self.fail(name)

    def test_stage_sockets_trailhead_and_goal_match(self):
        self.assertEqual(vectors(self.function("StageSpot")), [tuple(p) for p in self.layout["stages_cm"]])
        self.assertEqual(vectors(self.function("TrailheadSpot")), [tuple(self.layout["trailhead_cm"])])
        self.assertEqual(vectors(self.function("GoalSpot")), [tuple(self.layout["goal_cm"])])

    def test_candles_match_the_table(self):
        found = [tuple(float(v) for v in m) for m in re.findall(r"FVector\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)", self.function("Candles"))]
        self.assertEqual(found, [tuple(p) for p in self.layout["candles_cm"]])

    def test_every_stage_lies_on_the_map(self):
        (x0, x1), (y0, y1) = self.layout["map"]["x_cm"], self.layout["map"]["y_cm"]
        for x, y in self.layout["stages_cm"] + [self.layout["trailhead_cm"], self.layout["goal_cm"]]:
            self.assertTrue(min(x0, x1) < x < max(x0, x1) and min(y0, y1) < y < max(y0, y1), (x, y))


if __name__ == "__main__":
    unittest.main()
