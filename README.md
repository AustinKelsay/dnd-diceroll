# Evidence for PR #196 (#150 rigged cast batch 1: rogue and cleric)

Head `f91f21c`, PlebDev Ultra, UE 5.8.4.

- `sheet_01`-`sheet_05`: Blender preview sheets (`Art/rig-pilot/cast/previews/`): bind and Idle beside the fighter and wizard, rogue and cleric turnaround, all nine clips for each, and the party on dark stone.
- `10-pose-sheet-near.png`, `11-pose-sheet-default.png`: in-game (UE) captures from the new `pose` shot mode (`tools/capture/cast-poses.json`): Melee, Ranged, Cast, Hit, Downed and Death on the rogue (top) and the cleric (bottom), at the near zoom and the default zoom, cropped around the figure.
- `20`-`31`: default-zoom battle shots at yaw 0 and 180 on Stream trail (default seed), Root (`board-root-r002`) and Vigil (`board-vigil-pillars`), rigged (`-rigged`) next to the same shot with the static r001 figures (`-static`, `gallery --static-figures`).
- `contrast.json`: every measurement behind the contrast table in the PR (`tools/figure_contrast.py`).
