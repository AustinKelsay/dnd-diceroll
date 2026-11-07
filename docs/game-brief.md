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

Original fantasy setting; SRD 5.2.1 supplies the chosen open rules/content baseline. The supported subset and deliberate adaptations are listed in the rules reference. Linux Development on PlebDev Ultra is the validated target. The look is dark high fantasy with a low-poly PS1 influence, smooth and sharp rather than grainy: our own Blender-made characters, full surrounding worlds per encounter, and a candlelit tabletop for the mission overview. All assets are original with recorded provenance; engine primitives remain the fallback.

Broad character creation and leveling, multiplayer, official D&D setting IP, runtime AI narration, large narrative/faction simulations, open-world traversal, recruitment, resurrection, and persistent stat grinding are outside V1.

Issue #28 extends the historical V1 contract: each of the first four victories advances conscious survivors once, reaching level 5 for the finale. The fixed Fighter offers Champion or Battle Master before startup New Run. No new roles, respec, refill, revival or automatic rest. See [rules](rules-reference.md) for the Expedition adaptations.

Issue #40 adds a four-slot next-party builder for the nine currently implemented kits (five classes). Duplicate builds retain independent adventurer identities. Barbarian's four paths are playable from encounter one; Rogue/Cleric/Wizard base kits remain explicitly interim. This does not complete the approved eventual 13-class/70-subclass roster. See [current controls](runs/full-roster/03-ui-controls.md).
