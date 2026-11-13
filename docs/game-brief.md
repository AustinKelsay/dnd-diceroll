# dnd-diceroll — approved V1 brief

A single-player 3D tactical roguelike where the player leads four prebuilt adventurers through a short procedurally generated expedition. Party members persist within the run, actual deaths are permanent, and route choices shape the risks ahead.

## First playable version

- One simple objective: recover an artifact, completed by winning the finale.
- Five combat encounters including that finale; choices between route opportunities, with partial risk/reward information.
- Modular 3D tactical locations with room to maneuver (generated grids 16–18 × 11–12 cells). Generated configurations vary enemies, terrain, placement, objectives, and rewards while remaining valid.
- Fighter, rogue, cleric, wizard with a small, explicitly documented ability set.
- Overhead rotatable camera, grid movement, direct selection, reachable-cell/valid-target feedback, and readable action/roll results.
- Initiative, movement, attacks, defenses, damage, limited class abilities, downing, rescue, and permanent death from a defined SRD 5.2.1 subset.
- Health, ability resources, equipment, consumables, and encounter outcomes persist between fights. Equipment/consumables and survivor milestone levels provide progression under issue #28.
- One recovery opportunity for survivors, with a limited allowance and no resurrection.
- Dead adventurers remain lost; survivors continue. A party wipe ends the expedition. Starting roster only, with fresh party instances on a new run.
- Save/resume the current expedition. Historical run records persist without combat-power bonuses.

## Acceptance

A player can start, control all four roles, complete a battle, choose a route, carry consequences forward, recover once, finish or lose the expedition, and resume a saved in-progress run. A smaller surviving party can continue. The generated configuration is reproducible from its seed; action outcomes and random-generator state survive save/load. Illegal actions leave authoritative state unchanged.

## Scope

Original fantasy setting; SRD 5.2.1 supplies the chosen open rules/content baseline. The supported subset and deliberate adaptations are listed in the rules reference. Linux Development on PlebDev Ultra is the validated target. The look is dark high fantasy with a low-poly PS1 influence, smooth and sharp rather than grainy: our own Blender-made characters, full surrounding worlds per encounter, and a candlelit tabletop for the mission overview. All assets are original with recorded provenance; engine primitives remain the fallback. The graphics overhaul (#138) delivered that look; the next section describes what is built.

Broad character creation and leveling, multiplayer, official D&D setting IP, runtime AI narration, large narrative/faction simulations, open-world traversal, recruitment, resurrection, and persistent stat grinding are outside V1.

Issue #28 extends the historical V1 contract: each of the first four victories advances conscious survivors once, reaching level 5 for the finale. The fixed Fighter offers Champion or Battle Master before startup New Run. No new roles, respec, refill, revival or automatic rest. See [rules](rules-reference.md) for the Expedition adaptations.

Issue #40 adds a four-slot next-party builder for the nine currently implemented kits (five classes). Duplicate builds retain independent adventurer identities. Barbarian's four paths are playable from encounter one; Rogue/Cleric/Wizard base kits remain explicitly interim. This does not complete the approved eventual 13-class/70-subclass roster. See [current controls](runs/full-roster/03-ui-controls.md).

## The look now (graphics overhaul #138, integrated 2026-11-12)

The overhaul is presentation only: rules, RNG, saves and automation never depend on art (ADR-0001), and `-ExpeditionPrimitives` restores the engine-primitive look. It keeps the style (dark high fantasy, low-poly PS1 influence, smooth and sharp) and the rules (five-foot square cells, cardinal movement, Manhattan ranges, save schema 4). All art is original and scripted in Blender; provenance and the import recipe are in [assets](assets.md).

- **True scale.** A cell is 150 cm (five feet) and every standing figure is 175 to 185 cm (the brute and the warden are taller foes). Each world has one continuous ground with irregular paving that is not aligned to the cells, so the grid is no longer drawn all the time: it appears in movement and targeting as a reach contour, a path ribbon with destination brackets and hollow footprint rings, and a hold-to-show full grid for accessibility. Generated boards stay 16 to 18 x 11 to 12 cells; bigger arenas (#151) were decided against.
- **Camera and light.** A field of view of about 40 degrees with a pitch that follows the zoom, free orbit and a bounded pan. The default zoom is figure-sized (a standing figure about 90 to 110 px tall at 1080p) rather than the whole board; the wheel reaches the whole-board fit. Shadowed key light plus at most two shadowed local lights (braziers in a perimeter ring), vertex-baked glow only where the mask says so, and no Lumen, virtual shadow maps or Nanite. Party and enemy figures hidden behind architecture show as silhouettes (solid for the party, striped for enemies) and the overlays draw through it; the near-architecture mesh dithers out.
- **Worlds per encounter.** The Watchtower road is the Drowned Vigil (a cathedral nave with a sharp stained-glass rose), the Stream trail the Root of the World (a forest clearing with trunks, root arches and stone), and the finale, encounter 5, its own Vault: a roofless crypt hall with reliquary alcoves, sarcophagus rows and the cold blue Waystone in an apse on the back wall. Each has a landmark, secondary groups, medium and small props, three variants per obstacle recipe and 2-cell arches or walls for adjacent blocked pairs. A world that fails to load falls back as a whole to the earlier kit, never a mix.
- **A rigged cast.** All 13 classes are drawn as rigged, animated low-poly figures (3,000 to 5,100 triangles, two LODs) on one 25-bone skeleton, three body types (heavy, agile, robed) and ten shared clips (Idle, Walk, Melee, Ranged, Cast, Hit, Downed, Death, Recover and Brace), with a few per-character overrides (the ranger's and foe archer's bow draw, the barbarian's and brute's two-handed swing). Playback is driven by the committed command and never resolves an outcome, and skip, instant and Resume converge. Each of the 70 subclasses has its own palette (material parameters, no extra meshes), and a party with duplicate classes gets a small per-slot accent so the copies can be told apart. Spell attacks are class-tinted bolts released at the Cast frame; a non-spell ranged attack is a lightless arrow, so no hit or release blows out the figure.
- **Rigged foes.** The raider, scout and archer are rigged; the skirmisher, brute and warden exist as rigs and appear in play once encounter identity (#188) lands (the capture and dev flag `-ExpeditionFoeLooks` shows them today).
- **The candlelit overview.** The start, route and finale screens sit on a war table with a hand-drawn route map, candles, a bookcase and small props, with a diorama of each world (Root, Vigil and the Vault) on the table. The compact HUD (measured at about 14% of the screen in #146, against an 18% ceiling) keeps the board clear, and the d20 roll card sits in the left margin.

Known limits of the look (physical-pointer and pan/zoom coverage, human balance, rogues from behind, the skirmisher/brute/warden height and palette, Nanite/SM6, and perf measured on a headless nested output) are listed in the [graphics run record](runs/graphics-overhaul/README.md#remaining-limits-2026-11-12).
