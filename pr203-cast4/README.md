# PR #203 evidence: rigged cast batch 4 (bard, artificer, 70 subclass palettes, accents)

Head `220992e`. All images are rendered by the game (offscreen `gallery`, 1920x1080) unless noted.

| File | What it shows |
| --- | --- |
| `01_palette_contact_sheet_all_13_classes.png` | Every class (rows) and every subclass (columns, catalog order) in game, Idle, Root board, closest zoom |
| `02_duplicate_parties_accents.png` | `dup-four-fighters-*` and `dup-two-pairs-*` on Root, Vigil and the Vault: party-slot accents |
| `03_caster_bolt_tint_strip.png` | Six spell bolts in flight: wizard, cleric, sorcerer (crimson-magenta now), warlock, druid, bard |
| `04_...`, `05_fighter_vault_*_before_after.png` | The standing fighter on the Vault, `--no-palettes` against the warm-silver palette |
| `06_bard_and_artificer_in_game_poses.png` | Bard and artificer in game: Melee, Cast, Brace, Downed, Ranged |
| `sheet_23..27_*.png` | Blender bind/Idle, turnaround, clip sheets and the dark-stone party sheet |
| `contrast_per_build.md/json` | The gate table for all 70 builds (mean, worst, Vigil/Root/Vault, merge) |
| `fighter_before_after.json` | Fighter contrast numbers, before and after |
| `bolt_release_flash_failures.txt`, `flash/` | Bard and artificer release measurements (`impact_flash.py <shot>:shooter`) |
| `swatches.html` | The palette review page (resolved colours of all 70 builds) |

## Native chain at the head (each under the machine lock, `--cores 2`)

* `build`: 0 warnings
* `test --filter Project.Expedition.`: 315/315, 0 warnings
* `test --filter Project.`: 324/324, 0 warnings
* `test --filter Project.Tools.CaptureFixtures`: 1/1 (writes the 60 palette fixtures, the `cast4-*` fixtures and the rest)
* `gallery`: 18 runs, 684 pictures (162 skipped as not applicable); cast4 pose sets 4 fixtures x 2 resolutions, 272 pictures; cast4 release shots 52 pictures
* helper tests: 199 OK; `contribute.py check`: PASS; `palettes.py --check`: up to date
* headless import: added exactly `SK_Hero_{Bard,Artificer}`, `MI_SK_Hero_*`, `T_SK_Hero_*` (6); changed exactly `DA_DiceRollTheme` and `M_DiceRollTheme` (`dice-theme-7`); rerun: `THEME_MASTER kept`, `THEME_FILES_CHANGED 0`, hash diff of `Content/` and `Config/` empty. No earlier asset changed.

## Contrast gate (rigged Rim 1.2 / Fill 1.0, 15 default-zoom shots per build, Vigil/Root/Vault)

All 70 builds pass: mean >= 2.3 (lowest 2.46), worst shot >= 1.4 (lowest 1.44), every per-board mean >= 2.0 (lowest 2.10). See `contrast_per_build.md`.

## Fighter

Standing fighter (`pal-00-*`, 15 shots): mean 3.01 -> 3.06, worst 1.96 -> 2.04, Vault worst 2.17 -> 2.20. On the default `board-vault` fixture the fighter is Downed and drawn as the party silhouette fill: 1.43 without the palette, 1.42 with it (palette-independent; 1.38 at the batch 2 head).

## Bolt release (pure-white share of the shooter's box, `--max-white 0.01`)

Bard release and flight: at most 0.50% (Vault yaw 270 default); artificer arrow release: at most 0.92% (Vault near, the artificer is drawn as the silhouette fill behind an arch). Every box is under 1%. Peaks of about 254 are scene content (`bolt_release_flash_failures.txt` lists the boxes the tool flags on peak alone).
