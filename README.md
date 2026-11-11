# Evidence for PR #196 (#150 rigged cast batch 1: rogue and cleric)

First pass at `f91f21c`; review round 1 fixes at `9d62795` (PlebDev Ultra, UE 5.8.4).

- `sheet_01`-`sheet_05`: Blender preview sheets (`Art/rig-pilot/cast/previews/`), regenerated with the darker rogue: bind and Idle beside the fighter and wizard, turnaround, all nine clips for each, the party on dark stone.
- `10-pose-sheet-near.png`, `11-pose-sheet-default.png`: in-game captures from the `pose` shot mode (`tools/capture/cast-poses.json`), darker rogue: Melee, Ranged, Cast, Hit, Downed, Death for the rogue (top) and the cleric (bottom).
- `20`-`31`: default-zoom battle shots at yaw 0 and 180 on Stream trail, Root and Vigil. `-rigged` is the head `9d62795` (darker rogue); `-static` is the static r001 figures (`gallery --static-figures`).
- `40`: Vigil, rigged (first palette) left, static right (first pass).
- `41`, `42`: **before (first palette) left, after (darker) right** crops of the rogue on Vigil (yaw 0) and Root (yaw 180).
- `50-builder-*`: start-screen builder shots that select `rogue.assassin`, `rogue.thief`, `cleric.life` and `cleric.light`, at 1920x1080 and 1280x720. The builder is a card UI: it shows no figure for the selected class, only the class and subclass text over the tabletop.
- `contrast.json`: every measurement (image-region statistics; no figure mask) for the darker palette, the first palette and the static figures.
