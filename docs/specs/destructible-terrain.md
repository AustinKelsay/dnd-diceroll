# Destructible terrain with Chaos Destruction

**Status: draft, not approved.** Written 2026-11-01 from [the destruction research note](../research/destructible-environments.md); revised the same day after a six-seat council review (revision 2). Tracker: [#99](https://github.com/AustinKelsay/dnd-diceroll/issues/99) (`needs-triage`). The owner chose the "fanciest" presentation (Chaos Destruction) as the eventual target, said it is too early to implement, and asked for the spec first. Nothing here is scheduled. Product decisions that need the owner are collected in [Decisions for the owner](#decisions-for-the-owner); each carries a recommendation, but a recommendation is a proposal until the owner accepts it or explicitly delegates it. Neither implementation nor the throwaway spike starts without that.

Relationship to the environment roadmap (`docs/research/environment-roadmap.md`, a separate draft proposal): it places this feature as a later phase on a "breakable" terrain trait. If its phase 0 (terrain kinds with traits) lands first, the breakable state below becomes that trait plus hit points on a terrain piece. The targeting, resolution, restore, playback and Chaos contracts here do not change either way.

## Problem Statement

Every obstacle on the board is permanent. The player can only route around blocked cells, so terrain never answers to the party's choices, and a fight that opens with a wall between the wizard and the archer stays that way. The look is static too: hits leave no mark and nothing in the world reacts to the dice.

## Solution

Some walls become breakable. A breakable wall has armor and hit points like a creature, can be targeted by basic attacks in range and sight, and at zero hit points becomes rubble: a floor cell that no longer blocks movement or sight for either side. The rules decide all of this deterministically and the save records it. On screen the wall darkens as it takes damage and comes apart with Chaos Destruction when it breaks: a fractured Geometry Collection throws pieces, dust rises, the camera shakes, and the pieces settle over a hand-made rubble mesh. The Chaos layer is pure presentation: never read by the rules, never saved, and replaced by a mesh swap whenever the art, the physics scene or the renderer is unavailable.

Breaking a wall rarely shortens a path; the existing recipes already leave gaps. What it does is open firing lines both ways. That is the tactical trade the feature sells, and the interface has to show it.

## User Stories

1. As a player, I want to see which walls can be broken so I can plan around them.
2. As a player, I want to attack a breakable wall in range and sight with my basic attack, with the usual to-hit roll, dice tray and forecast.
3. As a player, I want a broken wall to open movement and sight for both sides so breaking it is a real trade.
4. As a player, I want a wall's remaining toughness readable before it breaks, on the wall and on hover.
5. As a player, I want to see on hover who would come into sight if this wall fell, and who would gain sight on me.
6. As a player, I want a breaking wall to shatter convincingly, with pieces, dust and a camera shake, and to settle into rubble that stays.
7. As a player, I want skipping or instant playback to land on the same rubble without waiting for physics.
8. As a player, I want save and resume to bring back the board exactly as it was broken, without replaying the collapse.
9. As a player, I want route previews to tell me when a layout has breakable walls.
10. As a maintainer, I want the rules, save format and automation independent of Chaos, plugins and rendered output.
11. As a maintainer, I want every fractured asset to come from this project's Blender sources with recorded provenance.

## Implementation Decisions

### Vocabulary

"Obstacle" already means any blocked cell in this codebase (the recipe comment, `BuildObstacle`, "three bounded obstacle recipes" in the rules reference). The new state is a **breakable**: `FBreakableState`, `Breakables`, `BreakableId`. In prose, "breakable wall". "Rubble" is a breakable at zero HP. "Blocked cell" keeps meaning permanent terrain.

### Rules (authoritative, plain C++)

- `FCombatEncounter` gains `TArray<FBreakableState> Breakables`. A breakable persists `Cell`, `Kind` (only `StoneWall` in this version) and `HP`; `ArmorClass` and `MaxHP` derive from the kind, and the id is the index in the list. `BlockedCells` keeps holding permanent terrain. A breakable with `HP > 0` blocks movement and sight exactly like a blocked cell; at `HP == 0` it is rubble and blocks nothing. Rubble stays in the list so restore validation, previews and presentation can see it.
- Sight to a breakable. `HasLineOfSight` tests every cell the ray enters including the destination (`CombatBattle.cpp` lines 213 to 226), so a standing wall would obscure itself. An attack on a breakable uses the same supercover traversal with its own cell as the destination excluded from the blocked test; every intervening blocked cell, standing breakable and the existing corner rule still apply. Tests: an adjacent wall, a wall behind another wall, a corner-grazing ray.
- Targeting. `FCombatCommand` gains `BreakableId`. `Attack` requires exactly one of `TargetId` and `BreakableId`; both set, neither set, a zero-HP breakable, or `BreakableId` on any other command type is rejected before any roll, with no state or RNG change. `LegalTargets` and `Forecast` stay creature-only, because `PlayEnemyTurn` attacks the first legal target (`CombatBattle.cpp` line 556) and party ids 1 to 4 would collide with breakable indices. New const queries `LegalBreakableTargets(ActorId)` and `ForecastBreakable(ActorId, BreakableId)` use the same range, sight and turn rules and spend no RNG.
- Resolution uses the creature order: d20, natural 1 misses, Champion's 19 to 20 critical applies (one rule for every attack roll), weapon die then critical die, damage bonus and gear rank, no Sneak Attack dice, no Precision Strike bonus or maneuver spend. A miss spends the Action and the d20 only. HP clamps at zero without calling `ApplyDamage`, which sets Downed or Dead. Magic Missile, Cure Wounds, Rescue and Potion never target breakables; Magic Missile's creature-only text and Fire Bolt's "creature or object" text are both SRD-faithful, so the only adaptation here is the toughness numbers.
- `FCombatResult` gains `TargetBreakableId`, `BreakableHPLeft` and `bBreakableDestroyed`; `TargetId` stays `INDEX_NONE` for breakable attacks. Rules never read these back.
- `SightIfBroken(ActorId, BreakableId)` returns two id lists for one actor: conscious enemies the actor would newly see, and conscious enemies that would newly see the actor, if that breakable were rubble. It is the same traversal with one cell cleared, spends no RNG and changes nothing. It belongs to the rules slice with a combat test, and the HUD reads it in the polish slice.
- Enemy turns never attack breakables in this version. `PlayEnemyTurn` reads the live board for pathing and sight, so a hole the party opens changes enemy options with no AI change. Test one deterministic example where a newly opened cell changes an enemy's move or target.
- Toughness is a dnd-diceroll adaptation of the SRD 5.2.1 object rules (Rules Glossary, "Breaking Objects": stone AC 17; hit points by size and resilience). The party attack bonus is +5 at every level (`CombatBattle.cpp` line 588), so AC 17 is a 45 percent hit chance on every Action. Expected cost in spent Actions, including misses, for the pairs the owner can pick from:

  | AC / HP | Level 1 martial (d6+3) | Level 5 martial (d6+7, no gear) | Note |
  | --- | --- | --- | --- |
  | 17 / 12 | about 4.4 | about 2.2 | two hits at level 1, one geared hit at level 5 |
  | 17 / 6 | about 3.0 | about 2.2 | recommended starting pair |
  | 15 / 12 | about 3.6 | about 1.8 | SRD wood AC |
  | 12 / 12 | about 2.9 | about 1.4 | raider AC, 70 percent hit chance |

  The wizard's d10 with no modifier is a little slower than the martials at level 1. The chosen pair is recorded in the rules reference as an adaptation; the exact numbers are delegated tuning once the owner picks the pacing target.
- Recipes decide which cells are breakable. The "broken wall" recipe has twelve cells: a column at x=7 with gaps at y=4 and y=7, plus five edge cells at (13..15,1) and (13..14,9), all drawn with the same `Wall` mesh. Recommended: all twelve are breakables, so "walls break, stones and pillars do not" is readable from the mesh alone (see decision 1 for the alternative). Breakable ids are a fixed function of the recipe, not of the run stream. "Split stones" and "ruined pillars" stay permanent. Recipes are drawn for every stage, so a breakable wall can appear in the vault finale.
- `ValidateEncounter` gates `Start` and `Restore` (`CombatBattle.cpp` line 26, used at line 595) and gains: breakable cells inside the grid, unique, disjoint from `BlockedCells`; valid kind; `0 <= HP <= MaxHP`; no combatant on a breakable with HP above zero; combatants on rubble allowed, because units walk onto it. Generation also keeps breakables off spawns. Breaking only ever opens cells, so existing connectivity holds.
- Route previews already name the terrain; the broken-wall preview says "breakable wall". Placement stays hidden.

### Persistence

- The save schema moves to the next unused version after whatever is integrated when this lands. Issue #39 (`ready-for-agent`) already claims version 4, so this is 5 if #39 integrates first. Every earlier version is rejected, no migration, explicit startup New Run preserves the old file beside the checkpoint, and the filename stays `current-v1.json`.
- The encoder writes every reflected property and the importer rejects a missing or extra field (`ExpeditionSave.cpp` lines 17 to 52), so adding `Breakables` changes the on-disk shape in the same commit. The rules change, the version bump, restore validation and the persistence tests are therefore one slice.
- `FExpeditionRun::Restore` regenerates the recipe and requires the saved breakables to match it in count, cells and kinds, with each HP in range. A saved breakable at zero HP restores as rubble with no replay.
- No presentation, physics or Chaos state enters a save.

### Presentation: playback

- `FExpeditionPlayback` gains `FBreakableDisplay` (id, shown HP, broken flag) beside `FUnitDisplay`. `Reset` and `Sync` build it from the snapshot: zero HP starts broken.
- `AddAttack` branches on `TargetBreakableId` before the creature lookup (today it returns early on a null target, `ExpeditionPlayback.cpp` line 226): the existing lunge or projectile, then a `BreakableHit` step (floating number on the cell, shown HP, small shake) or a `BreakableBreak` step (bigger shake, cue `Break`, broken flag).
- A new `OnBreak(BreakableId, Direction)` hook, suppressed while skipping like `OnShake` and `OnCue`. `Finish` runs every step's `Begin` with cues and shakes suppressed (`ExpeditionPlayback.cpp` lines 114 to 122, 501 to 511), so the broken flag alone cannot tell a played break from a skipped one; the view starts Chaos only from the hook. `Finish` and Instant speed set the broken flag and fire nothing.
- New cue `EExpeditionCue::Break`, synthesized like the others.

### Presentation: battle view

- Each breakable gets an `FExpeditionBreakableRig`: the intact themed `Wall` mesh with its tintable material, and the rubble mesh. `RebuildBoard` builds rigs from the snapshot; a broken breakable is built directly as rubble. Both paths consume identical draws from the board's seeded stream (draw the intact rotation and tint, then choose the mesh), so a resumed board shows the same rotations and decor as the live one (`ExpeditionBattleView.cpp` lines 1152 to 1160).
- Damage shows as a darker `Color` tint on the intact mesh (the theme material is opaque with only `Color` and `Glow`, so there is no crack mask or fade) plus the HP number on hover. Rubble appears by swap, with a scale pop rather than a fade.
- Targeting seams this feature must touch: `ActAtCell` submits a Move whenever a clicked cell has no combatant while Attack is selected (`ExpeditionPlayerController.cpp` lines 241 to 253), so a standing breakable on that cell submits Attack with `BreakableId` and rubble stays ordinary floor; the hover forecast binds only creature ids (`ExpeditionHUD.cpp` lines 637 to 642); the action bar disables Attack when creature targets and moves are both empty (`ExpeditionHUD.cpp` lines 594 to 595); the target-marker pool is fixed at eight (`ExpeditionBattleView.cpp` line 32) and must grow or fall back when walls and enemies are legal together. Keyboard targeting cycles breakables after creatures.
- Dust in the baseline is the existing sparkle system (`FPlaybackSparkle`, `SparkMeshes`) tinted as dust. It already obeys `Finish` and needs no new module.

### Presentation: the Chaos layer

- The Geometry Collection component is created at the break moment, never in `RebuildBoard`, and only when the world has a physics scene and the process can render. The presentation tests spawn the real view in a world created with `CreatePhysicsScene(false)` (`BattlePresentationTest.cpp` lines 220 to 229) and the component simulates by default and ensures a physics scene on registration, so a resident component would fail automation. The rig destroys the component with everything else on the next rebuild.
- On `OnBreak` at Normal or Fast speed the view hides the intact mesh, creates the collection from the kind's asset at the rig's transform with the damaged tint, breaks it with `ApplyExternalStrain` on that component only, above its damage threshold, and throws pieces with per-piece velocities away from the attacker (`ApplyBreakingLinearVelocity` applies one vector to one cluster, so it is not the radial impulse). No world fields, so neighbours cannot be strained. Damage from collision is off, `ClusterGroupIndex` stays 0 so collections never merge, and the rubble pops in underneath.
- Pieces need ground. Every board part is created with collision disabled (`ExpeditionBattleView.cpp` line 144) and the only collider is the sandbox floor 31 cm below the board top, so the view spawns one invisible collision-only box at board height over the grid, registered in `Spawned`, on a custom channel that only pieces collide with. Pieces ignore figures, other collections and everything else; picking is a plane test and is unaffected.
- Lifetime. Remove-on-sleep counts sleeping time, not time since the break, and the asset default `bRemoveOnMaxSleep` is false, so the asset record enables it and the view also enforces a hard lifetime after which the component is destroyed. `RebuildBoard` runs on route change and resume, not per command, so several walls can be settling together: the budget covers two overlapping breaks, and a third break settles the oldest first. `SkipPresentation`, `CycleSpeed` into Instant and `Reset` destroy any live collection and show rubble, even when playback is already idle (`Finish` returns at once when idle, `ExpeditionPlayback.cpp` line 503).
- Fallback is mandatory and its cases are listed: no theme entry, load failure, `-ExpeditionPrimitives`, no physics scene, cannot render, Instant speed, or a skip mid-break. Each is the mesh swap with dust sparkles. Automation under `-nullrhi` must never create a collection.
- `UDiceRollTheme` gains, per breakable kind, `TSoftObjectPtr<UGeometryCollection>` (fractured wall) and `TSoftObjectPtr<UStaticMesh>` (rubble). Both optional, like every other theme entry. A Niagara dust system is optional and only added if the spike shows the sparkles are not enough.

### Engine, plugins and modules

- Runtime module `DnDDiceRoll` adds `GeometryCollectionEngine`, which already depends publicly on `ChaosSolverEngine` and `FieldSystemEngine` (`GeometryCollectionEngine.Build.cs` lines 17 to 23). These three are engine modules under `Engine/Source/Runtime/Experimental/`. `Niagara` is a plugin module (`Engine/Plugins/FX/Niagara`, enabled by default) and is added only if the dust system is adopted.
- `DnDDiceRoll.uproject` lists `GeometryCollectionPlugin` and `Fracture` with `TargetAllowList: ["Editor"]`, matching the project's existing entries, because the Geometry Collection plugin depends on `EditorScriptingUtilities` (which this project scopes to Editor) and both ship runtime modules the game does not need. `ChaosEditor` already enables both by default for the editor. `Dataflow` is on by default and enables `ChaosCaching`; no cache asset or cache playback is used.
- Module and plugin changes alter the module manifest and cooked content, so the full gate in AGENTS.md applies once to the Chaos slice: build, bootstrap, automation, MCP/PIE, capture and Mac Development package, with the packaged plugin and module list and cooked size recorded before and after.
- Geometry Collections use their non-Nanite path; the project sets no Mac shader platform and Nanite is irrelevant to the look.
- Performance budget, agreed from the spike's numbers: frame time on the Mac Development build during a break and during two overlapping breaks must stay within an agreed margin of the current board, the hitch from creating a collection at runtime is measured, and every piece is gone within the hard lifetime. The performance skill measures against that budget on the same workload.
- Two hosts. Chaos is more exercised on Windows than on Mac. The second contributor may author fractures and test on Windows, but Mac Development verification stays the gate for every slice.

### Assets

- One Geometry Collection per breakable kind, authored in Fracture Mode or a Dataflow graph from a closed-volume derivative of the Blender wall. The current export omits the base's bottom face and builds exposed faces separately (`Art/dark-fantasy-sources/r001/scripts/build_props.py` lines 165 to 175), so the spike validates a watertight fracture source first, keeps the accepted wall mesh unchanged, and records the preparation in the asset recipe. Exterior and interior material slots use the wall's `MI_*` instance or a dark-stone child of `M_DiceRollTheme` so cut faces never show the engine default. Chunk count stays low (roughly 12 to 24 pieces, one cluster level), cuts planar or coarse Voronoi so rubble reads as hand-made low-poly.
- One rubble static mesh per kind from the same Blender scene, imported through the existing theme import script.
- Everything is original and project-owned, saved under `/Game/DnDDiceRoll/Theme/Props`, with versioned asset records and the fracture recipe (tool, seed, piece count, cluster and removal settings) per `docs/agents/asset-production.md` and `docs/assets.md`. No downloaded packs.

### Determinism contract

Chaos simulation is not deterministic across machines or frame rates and the project does not ask it to be. The committed breakable HP and the rubble state are the only facts. Pieces are never queried, never saved, never affect picking or pathing, and never delay, reorder or duplicate an outcome. Any test or review that finds rules or save code reading a physics or component value is a defect.

## Testing Decisions

- Combat, through the public command/result/snapshot seam under `Project.Expedition.Combat`: legal and illegal breakable attacks (turn, Action, range, sight with the endpoint rule, rubble, wrong command, both targets set); hit and miss against breakable AC; RNG consumed in the creature order; damage and remaining HP; destruction opening `LegalMoves` and sight for both sides; Sneak Attack and Precision Strike refusal; `LegalTargets` never containing breakables; `ForecastBreakable` equal to resolution with zero RNG spend; `SightIfBroken` on a worked layout; rejection preserving snapshot and RNG; one enemy-turn example that changes after a break. Catches rules errors; establishes nothing about rendering.
- Run, through the public run seam: the broken-wall recipe generates breakables on valid cells, previews name them, other recipes generate none, seeds reproduce the layout. `LargeGridsKeepPlayableOpeningGap` (`ExpeditionRunTest.cpp` line 66) and the connectivity flood fill (lines 166 to 198) count standing breakables as blocked.
- Persistence: round trip with damaged and destroyed breakables and a unit standing on rubble, continued commands after restore, rejection of every earlier version, of breakables that disagree with the recipe, and of HP outside range, while preserving the last valid save.
- Presentation: `Project.Expedition.Presentation.PlaybackConvergesOnCommittedState` runs in a world without a physics scene and spawns the real view. Extend it with a broken-wall layout so a queued break converges to rubble under `Finish` and under Instant, a resumed zero-HP breakable starts as rubble, `OnBreak` never fires while skipping, and no Geometry Collection component exists afterwards. Playback-only tests cover the step order.
- Rendered evidence lands with the slice that introduces the behavior: the rules slice captures a wall targeted, hit and broken by mesh swap; the polish slice captures the tint, HP hover, sight preview and a resumed rubble fixture from `Project.Tools.CaptureFixtures`; the Chaos slice captures a break at Normal speed, the same break skipped mid-tumble, two overlapping breaks, and the `-ExpeditionPrimitives` path, plus a live-versus-resumed board comparison.
- The engine gate runs once, on the Chaos slice: fresh bootstrap, `-nullrhi` automation, identity and discovery, PIE through MCP, captures, Mac Development package with cooked size and module list recorded before and after, and packaged startup into a break.
- The spike produces a short run note with captures and numbers, not tests.

## Slices

Thin vertical tickets in dependency order. Each is one issue with the parent spec as its parent once approved.

| Slice | Outcome | Blocked by | Notes |
| --- | --- | --- | --- |
| 0. Chaos spike | A throwaway branch in an isolated worktree and a sandbox map fractures a watertight derivative of the theme wall, breaks it from a key press, and answers: does the look fit at the game's camera distance; how interior faces render with the theme material; whether the collection renders on Metal; frame time and creation hitch on the Mac Development build; whether `-nullrhi` automation and the Mac package survive the module link; packaged plugin and module list and cooked size before and after; whether a skip mid-tumble snaps cleanly. Output is a run note with captures and a go or no-go on piece style. | owner go-ahead | No merge of binaries; only the note. Runs when Chaos work is scheduled and gates slice 3 only. Approval of this spec does not wait for it. |
| 1. Breakable walls, end to end | Breakables in the rules, sight endpoint rule, targeting queries and command contract, result facts, `SightIfBroken`, broken-wall recipe, previews, encounter validation, the save version bump with restore validation, the controller and HUD seams above, standing walls drawn with today's mesh and rubble by swap, combat, run and persistence tests. | #39 coordination for the version | One slice because the save shape changes with the rules. Delivers a playable feature with no Chaos. Coordinate with #39 so the kind rides the catalog if it lands first. |
| 2. Presentation polish | Damage tint, HP on hover, sight-if-broken preview, break step and cue, dust sparkles, target-marker growth, fixture capture. | 1 | Overlaps #50 in `ExpeditionHUD.cpp`; whichever lands second rebases. Not a blocking edge while #50 is `needs-triage`. |
| 3. Chaos layer | Module and plugin changes, Geometry Collection and rubble for the wall, ground collider, break trigger, lifetime and skip cleanup, fallbacks, full engine gate, budget measurement. | 0, 2 | The fancy part. Whether the feature is accepted without it is the owner's call (decision 8), not an implementer's. |

## Decisions for the owner

Each with a recommendation. A recommendation is a proposal; nothing proceeds on an unanswered item.

1. **Which cells break.** Recommended: all twelve wall-mesh cells of the broken-wall recipe, so breakability is readable from the mesh alone and no new visual language is needed. Alternative: only the seven-cell column at x=7, with the five edge cells kept permanent and given a different mesh or tint. Only the column cells at y=3, 5 and 6 open party-to-enemy sight from the opening rows; the end cells widen lanes.
2. **Do enemies break walls.** Recommended: no, in this version. It keeps enemy turns readable and avoids an AI change; the party owns the new verb.
3. **Which actions damage walls.** Recommended: basic Attack only (fighter melee, rogue and cleric ranged, wizard Fire Bolt). Magic Missile stays creature-only, which is the SRD text; Sneak Attack and Precision Strike never apply.
4. **Wall toughness, as a pacing target in spent Actions.** Recommended: SRD stone AC 17 with 6 HP, about three Actions at level 1 and about two at level 5. At 12 HP the level 1 cost is about four and a half Actions, more than a full party round, which may make the verb not worth using. Final numbers are tuning against the target the owner picks.
5. **Preview disclosure.** Recommended: the route preview names "breakable wall", placement stays hidden, consistent with partial risk/reward information.
6. **Rubble and movement.** Recommended: rubble is plain floor. Difficult terrain (#64) and cover (#63) are separate foundations; only their later integrations with rubble and breakables depend on this spec.
7. **Finale and frequency.** The recipe is drawn per stage, so breakable walls can appear in the vault finale and a whole expedition can pass without one. Recommended: accept both for now and revisit after play.
8. **Is the mesh-swap feature acceptable on its own?** Slices 1 and 2 deliver the whole feature without Chaos. Recommended: keep Chaos as the intended presentation and treat mesh swap as the mandatory fallback and development milestone; dropping slice 3 after a failed spike or Mac gate is a separate owner decision, not an implementer default.
9. **Fracture style.** Decide after the spike captures: coarse planar cuts, coarse Voronoi, or hand-cut chunks from Blender. Recommended: whichever reads as chunky low-poly at the game's camera distance, with the fewest pieces.

## Adjacent specs

Filed as separate `needs-triage` ideas (#100 to #104) with the blocking edge named. None is part of this approval.

| Idea | Outcome | Blocked by |
| --- | --- | --- |
| Cosmetic battle damage (#100) | Scorch on hit cells, rubble sprinkles on kills, a crumble on enemy death. Zero rules or save impact, keyed to existing result facts, safe under `Finish`. Carries no engine gate as long as it uses no Chaos. | none; presentation-polish stream |
| Props and hazards (#101) | Barrels, crates and cages as breakable kinds, including fragile kinds that break with no roll (SRD "Breaking Objects"); a brazier that burns adjacent cells when it falls, as the first hazard. Changes the recipe generator and adds the no-roll resolution when it has a consumer. The theme already has brazier, flame and cage meshes. | #99 slice 1 |
| Scripted collapse and timed hazards (#102) | The recipe schedules "round N: these cells fall or deal damage", resolved in `AdvanceTurn` with the seeded stream, named in previews. | #99 slice 1 |
| Area effects against terrain (#103) | An area spell removing several breakables in one result. The spell is the product decision; the terrain effect follows. | #75, #99 slice 1 |
| Finale set piece (#104) | A collapse or shatter around the Waystone pedestal on victory. Presentation only unless vault walls are breakables. | #102 or #100 |
| More breakable kinds | Stones and pillars as breakables if the owner wants them after playing the wall. | #99 slice 3 |
| Destructible cover and rubble as difficult terrain | Integrations only. #63 (cover) and #64 (difficult terrain) stand on permanent terrain and keep their own dependencies; this row is the later work that makes a breakable count as cover while it stands and rubble count as difficult ground. | #63 or #64, plus #99 slice 1 |

## Out of Scope

Enemy attacks on breakables, destructible props and hazards, area effects (#75), scripted collapses and timed hazards, cover modifiers (#63), difficult terrain (#64), fragile no-roll kinds, Chaos caches, Chaos Flesh and cloth, networking, Nanite, non-Mac targets, and any change to creature combat math.

## Further Notes

Proposed glossary terms once approved: **Breakable** (a board piece with armor and hit points that blocks movement and sight while it stands), **Rubble** (a breakable at zero hit points; plain floor that keeps its entry for presentation and validation). **Obstacle** and **blocked cell** keep their current meanings.

Related ideas in the tracker: #63 cover, #64 difficult terrain, #65 forced movement, #75 areas of effect, #50 range tiles and target chips, #39 catalog-backed content and save version 4. The research note records the Unreal facts behind every engine decision here, including which plugins are on by default in 5.8.4 and why the pieces cannot be the committed state.
