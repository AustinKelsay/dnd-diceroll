# Evidence for PR #206: duplicate accents read on rogues and paladins (#150)

Code head `9f5e7046`, PR head `2c3ac779` (docs only on top). Branch from `origin/staging` `02f4e01` (has #204). 1920x1080, default zoom, offscreen, rigged cast, `--cores 2` under `/home/plebdev/.t3/ultra-unreal.lock`; nothing near `wayland-1`.

## Before and after, four copies on Vigil, Root and the Vault
- `sheets/before-yaw000.jpg` / `after-yaw000.jpg`: from the front (top row four paladins, bottom row four rogues, boards left to right Root, Vigil, Vault), 200% crops of the game's own render.
- `sheets/*-yawm90.jpg`: the game's home yaw (-90, side-on, the party in a line); `sheets/*-yaw180.jpg`: from behind.
- `frames/`: the full 1920x1080 frames (JPEG) of each fixture and yaw.

Judged by eye: can each of the four copies be named by one colour without hunting? Before, no for rogues (a few wrist pixels) and barely for paladins (crest and trim specks). After, from the front: yes for both on all three boards (rogue sash gold / green / blue / red and tinted hood; paladin shield face and boots gold / green / blue / red). From behind the paladins differ too (shield back, haft, boots). Rogues from behind and side-on stay hard (the cloak is the cloth base tile): a known limit.

## Measurements (`tools/accent_share.py`, same four copies, same 20 views)
| | share | patch per copy | copies apart (mean dE) | nearest pair |
| --- | --- | --- | --- | --- |
| rogue before | 5.7% | 97 px | 29.4 | 12.9 |
| rogue after | 7.8% | 130 px | 40.0 | 16.7 |
| paladin before | 8.1% | 146 px | 31.4 | 13.2 |
| paladin after | 7.8% | 166 px | 47.0 | 10.1 |

Front view (yaw000) rogue: 112 px, dE 36 -> 327 px, dE 65. Home yaw (-90) paladin: 175 px, dE 31 -> 411 px (shield face-on), dE 67. The measure looks at the middle half of each footprint box, so the paladin's shield and boots read 0 px from the front there; the sheets show them. `measure/before-accent-share.*`, `after-accent-share-13-classes.*` (all 13 classes, `--max 0.10 --max-worst 0.25`, exit 0; largest means sorcerer 8.5%, druid 8.0%, rogue and paladin 7.8%; largest single views rogue 24.1% (front-on sash), paladin 21.5%, sorcerer 17.7%).

## Contrast gate over the duplicate parties (`palette_report.py --duplicates`, 60 measurements per class)
fighters 2.83 / worst 1.78 (Vigil 2.83, Root 2.54, Vault 3.12), paladins 3.25 / 1.61 (3.14, 2.93, 3.67), rogues 2.63 / 1.67 (2.63, 2.33, 2.93): pass (floors 2.3 / 1.4 / 2.0 per board). `measure/duplicate-party-contrast.*`.

## Default party
`default-seed-battle` rendered with and without `--no-accents`: the same differences as two identical renders (home-yaw frame 3,146 vs 3,860 pixels of render noise; the others 0 to 28), so a party with no duplicates draws nothing from the accent. `measure/inl.diff` shows the generated table changes only the rogue and paladin accent row.

## Native chain
`logs/`: build 0 warnings; `Project.Expedition.` 315/315; `Project.` 324/324 (0 warnings); `Project.Tools.CaptureFixtures` 1/1; `gallery` 684 pictures (162 skipped as not applicable); the 12 duplicate fixtures (fighters, paladins, rogues, two pairs on 3 boards x 5 default-zoom shots). Helper tests 222 OK on Python 3.11 (`uv run --python 3.11 --no-project`) and 3.14; `contribute.py check` PASS; `palettes.py --check` up to date.

## Limits
- Rogues from behind / side-on (needs a hem tile split, not done).
- Rogue worst single view 24.1% is above the old 22.5% worst-view default; class mean 7.8%.
- Not re-run: perf (the lead's), the packaged on-screen pass (the lead's), Resume (the party slot is unchanged and no checkpoint field changed).
