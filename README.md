# Evidence for #218 PR B (head abc1223d, base staging 3a5eae1)

- `captures/<WxH>/<fixture>/`: the `reward` capture set (`tools/capture/reward-poses.json`) on the `reward-choice-*` fixtures at 1280x720, 1920x1080 and 1600x600 (JPEG q85 of the gallery PNGs, with each run's manifest). Each was inspected.
  - `reward-choice-first`: reward 1. Each card is focused in turn (`reward-gear`, `-reserve`, `-honed`). Reserve is shown on the Life cleric (Kit + Slot), the Barbarian (Rage) and the Battle Master (Wind + Maneuver).
  - `reward-choice-upgraded`: reward 3, where hero 4 holds Reserve and Honed. The `reward-taken-*` shots show both blockers.
  - `reward-choice-gear-cap`: reward 4, where hero 4 is at gear rank 3 (`Gear at rank 3: potions only`). Reserve shows no refill (Wind 0/2 → 1/3), and fallen heroes read `fell at LvN`.
  - `reward-choice-fallen`: a reward with a fallen hero.
  - `reward-choice-camp`: the camp after Reserve at reward 1. Each survivor's line is at its own cap.
  - `reward-choice-route`: the revision 10 route cards (`1 upgrade choice, +N potion(s)`).
  - Skipped shots (a card on the camp or route) are listed in the manifests.
- `gate/`: the zero-warning build log and the full `Project.` report at the head (374/374, 0 warnings).
