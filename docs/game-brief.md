# Expedition — approved V1 brief

A single-player 3D tactical roguelike where the player leads four prebuilt adventurers through a short procedurally generated expedition. Party members persist within the run, actual deaths are permanent, and route choices shape the risks ahead.

## First playable version

- One simple objective: recover an artifact, completed by winning the finale.
- Three combat encounters including that finale; choices between route opportunities, with partial risk/reward information.
- Small modular 3D tactical locations. Generated configurations vary enemies, terrain, placement, objectives, and rewards while remaining valid.
- Fighter, rogue, cleric, wizard with a small, explicitly documented ability set.
- Overhead rotatable camera, grid movement, direct selection, reachable-cell/valid-target feedback, and readable action/roll results.
- Initiative, movement, attacks, defenses, damage, limited class abilities, downing, rescue, and permanent death from a defined SRD 5.2.1 subset.
- Health, ability resources, equipment, consumables, and encounter outcomes persist between fights. Equipment/consumables provide V1 progression; leveling follows later.
- One recovery opportunity for survivors, with a limited allowance and no resurrection.
- Dead adventurers remain lost; survivors continue. A party wipe ends the expedition. Starting roster only, with fresh party instances on a new run.
- Save/resume the current expedition. Historical run records persist without combat-power bonuses.

## Acceptance

A player can start, control all four roles, complete a battle, choose a route, carry consequences forward, recover once, finish or lose the expedition, and resume a saved in-progress run. A smaller surviving party can continue. The generated configuration is reproducible from its seed; action outcomes and random-generator state survive save/load. Illegal actions leave authoritative state unchanged.

## Scope

Original fantasy setting; SRD 5.2.1 supplies the chosen open rules/content baseline. The supported subset and deliberate adaptations are listed in the rules reference. Mac Development is the validated target. V1 uses readable simple visuals and engine-native assets with provenance.

Character creation, leveling, multiplayer, official D&D setting IP, runtime AI narration, large narrative/faction simulations, open-world traversal, recruitment, resurrection, and persistent stat grinding are outside V1.
