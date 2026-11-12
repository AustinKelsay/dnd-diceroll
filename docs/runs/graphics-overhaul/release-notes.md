# Graphics overhaul: release notes

For Austin, 2026-11-12. This is the plain-language summary of the whole overhaul ([#138](https://github.com/AustinKelsay/dnd-diceroll/issues/138)) as it stands on `staging` (`f09abfe`, the last feature merge), with the acceptance run for [#152](https://github.com/AustinKelsay/dnd-diceroll/issues/152). The numbers and the ticket-by-ticket record are in the [run record](README.md). Since the last release to `main` (the 2026-11-10 candidate `0f3042e`, PR #195) `staging` adds everything from the Vault world, the tabletop and the full rigged cast to the foes; the earlier overhaul work (grid, camera, lighting, the Vigil and Root worlds, the rigged pilot, the HUD) shipped in that release.

Nothing here changes rules, saves, the dice or the balance. The look is all presentation, and `-ExpeditionPrimitives` still gives you the old engine-shape look.

## What you will see

**Every encounter has a full world, to true scale.**
- A cell is a real five feet (150 cm) and every hero is about 180 cm, so a figure is a person on a floor, not a token on a tile. The floor is one continuous stone or moss ground with irregular paving, and the grid only appears when you move or aim (a reach outline, a path ribbon with a destination bracket, rings under the feet; hold a key to see the full grid).
- The Watchtower road is the Drowned Vigil (a cathedral nave with a stained-glass rose that is now sharp), the Stream trail is the Root of the World (a forest clearing with trunks, root arches and stones), and the finale is a new Vault: a roofless crypt hall with alcoves, sarcophagi and a cold blue Waystone in an apse at the back.
- Pillars, walls and rubble come in variants, adjacent blocked cells become 2-cell walls or arches, and anything that would hide your units fades or shows them as a silhouette (solid for your party, striped for enemies).
- Boards are still 16 to 18 by 11 to 12 cells. I looked at 24x16 and decided against it (see below).

**The whole cast is rigged and animated.**
- All 13 classes are drawn as low-poly rigged characters (fighter, wizard, rogue, cleric, barbarian, monk, paladin, ranger, sorcerer, warlock, druid, bard, artificer) with their own gear: the rogue's hood and twin daggers, the cleric's mace and round shield, the barbarian's fur and greataxe, the ranger's bow, the sorcerer's crackling crystal, the warlock's eye-sigil tome, the druid's bark and leaf, the bard's lute and rapier, the artificer's boiler pack and goggles.
- They walk, swing, shoot, cast, flinch, fall, get rescued and get up. A spell is a coloured bolt (each caster has its own hue), a bow shot is a plain arrow, and neither blows out the screen when it leaves or lands (an early problem that was fixed).
- Each of the 70 subclasses has its own palette, and if your party has two or more of the same class each copy gets a small accent colour (a sash and hood for rogues, a shield and boots for paladins, and so on), so you can tell them apart.

**Foes are rigged too.** The raider, scout and archer are now animated figures. Three new foe looks, the skirmisher, the brute (two-handed maul) and the warden (Vault guardian), are built and tested but will only appear in real fights once the encounter work in #188 lands.

**The mission overview is a candlelit war table.** The start, route and finale screens sit on a table with a hand-drawn route map, candles, a tankard, dice, scrolls and a bookcase, with a small diorama of the forest, the nave and the Vault standing on it. The HUD is compact and the d20 roll card sits in the left margin.

**It still runs fast.** On the packaged game at 1920x1080, three full runs of the same fight gave a 95th-percentile frame of 3.36 to 3.38 ms (budget 4 ms), GPU 2.67 to 2.69 ms (3.5), game thread 1.19 to 1.20 ms (1.5) and 1243 to 1252 MiB of memory (1.5 GiB). That is a little over 0.2 ms slower than the first vertical slice (gate 2) and about the same as the last release. The worst frame during the fight was 6.6 ms; the two earlier one-off stalls (a 2.5 s frame and a 46 ms cluster) did not come back.

## Screenshots

- The Vault finale world, three zoom levels: [contact sheet](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr197-vault/vault-contact-sheet-1080-default.jpg)
- Root of the World: [forest clearing](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr197-vault/themes/board-root-r002-yawm90-default.jpg); the Drowned Vigil: [nave with broken walls](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr197-vault/themes/board-vigil-walls-yawm90-default.jpg)
- The war table: [the finale screen](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr199-tabletop/blender/game_finale.jpg) and the [Vault diorama close-up](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr199-tabletop/closeups/finale-left-diorama-vault.jpg)
- The cast: [barbarian, monk, paladin and ranger poses](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr198-cast2/11-pose-sheet-default.png) and [all 13 classes with their palettes](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr203-cast4/pr203-cast4/01_palette_contact_sheet_all_13_classes.png)
- Duplicate classes told apart: [fighters, paladins, rogues and pairs](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr203-cast4/pr203-cast4/02_duplicate_parties_accents.png), [four paladins in the forest](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr206-accents/pr206-accents/frames/after-dup-four-paladins-root-yaw000.jpg), [four rogues in the forest](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr206-accents/pr206-accents/frames/after-dup-four-rogues-root-yaw000.jpg)
- Caster bolts, one hue each: [the strip](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr203-cast4/pr203-cast4/03_caster_bolt_tint_strip.png)
- The foes in the Vault: [brute, warden and archer beside the Waystone](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr205-foes/pr205-foes/combat/foes-vault-finale-yaw000-default.png) and [on the Vigil](https://github.com/AustinKelsay/dnd-diceroll/blob/evidence/pr205-foes/pr205-foes/combat/foes-vigil-b-yaw000-default.png)
- Gate 2's rough edges, for comparison: [the slice report](https://github.com/AustinKelsay/dnd-diceroll/tree/evidence/onscreen-slice-871185a)
- Acceptance run (perf reports, gallery indexes, a few captures): @@EVIDENCE@@

## What was decided

- **Bigger arenas (#151): no.** The arenas stay 16 to 18 by 11 to 12. The default zoom is figure-sized now, so a bigger board would mean more panning and travel turns, not bigger figures; the encounter plan (#188) is built on today's sizes; and a change would reopen the Compact balance baseline. Room to flank comes from layouts and the new architecture. If you want bigger maps anyway, reopen #151.
- **Rogues from behind.** Four rogues are told apart by their sashes from the front and sides, but from behind they are still hard to tell apart (a cloak-hem change was deferred; see below).
- **CI mistake, fixed.** One cast PR (#203) was merged with a red check; the cause was a Python 3.11 test issue and #204 fixed it. The rule is now: check CI immediately before merging.

## What is not covered

- **Not played by a person.** Every on-screen pass used scripted keyboard and pointer input on a headless test display. A real mouse (wheel, drag orbit, pan and zoom feel), human balance, fun and readability over a whole run are unchecked, and so are timings on a physical monitor.
- **Mac.** The Mac crashes (#32, #35) are unresolved and nothing was run on a Mac. Linux on PlebDev Ultra is the only validated host.
- **Linux test-shutdown crash (#157)** after a failed test run, and the gallery shader crash (#174, reduced but not proved gone), are still open.
- **Rogues from behind** (a repaint and new UVs would fix it), and the **skirmisher, brute and warden** will need their height and colour tuned once #188 puts them in fights (the brute is barely taller than your tallest hero, and the warden's pale stone is close to the cleric and paladin).
- **HUD (#200):** on the route screen the hero-details panels for slots 2 to 4 draw under the route banner and the left card.
- **Nanite and Shader Model 6 are not certified** (#193 and #105): Linux runs Shader Model 5, where Nanite is off.
- The complete list, with the smaller items, is under [Remaining limits](README.md#remaining-limits-2026-11-12).

## How it was checked (at `f09abfe`)

Build with 0 warnings; 323 Expedition tests and 332 project tests passing with 0 warnings; the capture-fixture test; 235 helper tests on Python 3.11 and 3.14; the repository check; a Linux Development package; the full capture gallery (@@GALLERYSHORT@@); and the perf gate three times. #205 and #206 were merged back to back and both touched the same test fixture file, so their combination was built and tested first (clean). Details and numbers are in [Integrated acceptance](README.md#integrated-acceptance-152).

## Next

1. Review and merge the #152 PR; close #150, #152 and #138.
2. Promote `staging` to `main`.
3. #188 (encounter identity) brings the skirmisher, brute and warden into play; tune their height and colour then, and decide on the rogue's cloak-hem tile split.
4. A real-mouse, real-monitor playthrough when you have time.
