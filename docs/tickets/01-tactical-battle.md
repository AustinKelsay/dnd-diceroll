# Play a tactical battle

## Parent spec

[Expedition V1](https://github.com/AustinKelsay/expedition-rpg/issues/1). Slice 1; no blockers.

## Outcome

A player can enter a rendered 3D tactical battle, control four prebuilt adventurers, move and attack on their turns, watch bounded legal enemy turns, and reach victory or defeat. This proves the combat command/state boundary and its presentation before expanding the run.

## Acceptance criteria

- Four identifiable party members and an enemy group occupy valid, nonoverlapping cells on a small walkable grid.
- A rotatable overhead camera, visible input instructions, selection, turn/health state, reachable cells, target previews, and dice/action feedback make the battle playable.
- Movement, attacks, and end-turn requests validate phase, ownership, range, occupancy, and action budgets before mutation. Rejections preserve state and RNG.
- Initiative, hit/miss/critical attacks, damage, inactive zero-health units, bounded enemy decisions, and battle terminals behave deterministically under a seed. Detailed downed/rescue/permanent-death rules belong to slice 2.
- Rules own stable IDs and snapshots; actors and UI cannot bypass them. Preserve the existing runtime build module and sandbox asset.
- Automation through public behavior seams exercises legal/illegal actions, RNG preservation, turn progression, attack outcomes, enemy completion, and both battle endings.
- A real editor build and rendered PIE observation establish startup, input, camera, HUD, legal actions, and clean teardown. Report precise limitations rather than inferring input from state-only tests.

## Implementation and review

Use the approved spec and architecture decisions, behavior-first tests where practical, and two-axis Standards/Spec review against the slice's fixed baseline. Commit the reviewed slice on the shared feature branch; no PR or remote pushes from the worker. Keep later roles, run routing, and persistence out of this slice.

Tracker: https://github.com/AustinKelsay/expedition-rpg/issues/2
