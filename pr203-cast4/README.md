# PR #203 evidence: rigged cast batch 4 (bard, artificer, 70 subclass palettes, accents)

Head `2aba698` (review round 1: strict gates, stronger paladin and rogue accents, a distinct Swarmkeeper).

| File | What it shows |
| --- | --- |
| `01_palette_contact_sheet_all_13_classes.png` | Every class (rows) and every subclass (columns, catalog order) in game, Idle, Root board, closest zoom |
| `02_duplicate_parties_accents.png` | `dup-four-fighters-*`, `dup-four-paladins-*` and `dup-two-pairs-*` on Root, Vigil and the Vault: small party-slot accents, class palette kept |
| `accent_share_per_class.txt/json`, `duplicate_parties_contrast.json` | The accent region of each of the 13 classes measured in game (changed pixels over visible figure pixels, accent on vs off), and the contrast of the duplicate parties |
| `03_caster_bolt_tint_strip.png` | Six spell bolts in flight, hue-spread: wizard orange 32, cleric gold 48, sorcerer magenta 329, warlock violet 275, druid leaf green 110, bard teal 170 |
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
* `test --filter Project.`: 324/324, 0 warnings (head `b57c919`)
* `test --filter Project.Tools.CaptureFixtures`: 1/1 (writes the 60 palette fixtures, the `cast4-*` fixtures and the rest)
* `gallery`: 18 runs, 684 pictures (162 skipped as not applicable); cast4 pose sets 4 fixtures x 2 resolutions, 272 pictures; cast4 release shots 52 pictures; spell and cast3 release shots re-measured for the new tints
* helper tests: 199 OK; `contribute.py check`: PASS; `palettes.py --check`: up to date
* headless import: added exactly `SK_Hero_{Bard,Artificer}`, `MI_SK_Hero_*`, `T_SK_Hero_*` (6); changed exactly `DA_DiceRollTheme` and `M_DiceRollTheme` (`dice-theme-7`); rerun: `THEME_MASTER kept`, `THEME_FILES_CHANGED 0`, hash diff of `Content/` and `Config/` empty. No earlier asset changed.

## Contrast gate (rigged Rim 1.2 / Fill 1.0, 15 default-zoom shots per build, Vigil/Root/Vault)

All 70 builds pass with every build x board x shot present (15 each; the report now fails on anything missing): mean >= 2.3 (lowest 2.46), worst shot >= 1.4 (lowest 1.44), every per-board mean >= 2.0 (lowest 2.10). The Swarmkeeper (honey-gold cloak over a dark tunic) 2.95 / 1.91 against the Hunter's forest green. See `contrast_per_build.md`.

## Accent region (lead's call: a small accent, never a base tile)

`tools/accent_share.py` on `acc-<class>-root` (four heroes of one build of each class), `gallery` with and without `--no-accents`, 20 default-zoom hero views per class: changed pixels over the figure's visible pixels. Class means: artificer 7.0%, barbarian 3.9%, bard 6.6%, cleric 6.2%, druid 7.9%, fighter 7.9%, monk 4.4%, paladin 8.1%, ranger 4.1%, rogue 5.6%, sorcerer 8.2%, warlock 6.3%, wizard 5.7%. Largest single hero view 17.7% (sorcerer, accepted). The tool now fails on any missing class, run, shot or hero. The tile lists exclude plate, cloth and skin and are asserted in `tools/tests/test_palettes.py`. Duplicate parties on Vigil, Root and the Vault keep the gate: fighters mean 2.83 (worst 1.78), paladins 3.23 (1.62), clerics 3.54 (1.98), rogues 2.66 (1.60). The paladin's accent is dark and fully saturated and the rogue's deep and vivid, inside the same small regions.

## Fighter

Standing fighter (`pal-00-*`, 15 shots): mean 3.01 -> 3.06, worst 1.96 -> 2.04, Vault worst 2.17 -> 2.20. On the default `board-vault` fixture the fighter is Downed and drawn as the party silhouette fill: 1.43 without the palette, 1.42 with it (palette-independent; 1.38 at the batch 2 head).

## Bolt release (pure-white share, `--max-white 0.01`)

Shooter boxes: bard at most 0.50%, cleric 0.42%, wizard 0.82%, druid 0.18%, sorcerer 0.07%, warlock 0.23%, artificer arrow release 0.92%. The bolt's own box: warlock at most 0.97% (it was 2.3% with a brighter violet; the tint was dimmed), every other bolt 0%. Peaks of about 254 are scene content (`bolt_release_flash_failures.txt` lists boxes flagged on peak alone).
