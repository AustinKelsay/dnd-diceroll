# dnd-diceroll

The approved game is a single-player 3D tactical roguelike. One player controls four prebuilt adventurers through a short generated expedition. The world is original fantasy, with a deliberately supported subset of SRD 5.2.1 rules.

## Approved product decisions

- A campaign is one expedition/run: one straightforward objective, five combat encounters including a finale, route choices, and one recovery opportunity.
- The starting party contains a fighter, rogue, cleric, and wizard. Identities, health, resources, equipment, and deaths persist within the run.
- Zero health means downed with a limited rescue opportunity. Actual death is permanent; survivors continue. A full party wipe ends the run. No replacement recruitment in V1.
- A new run starts fresh. Records and player knowledge persist, but combat power does not.
- Rotatable overhead camera, direct party selection, tactical grid, readable movement/target/action feedback.
- Generation uses world/run state and previous choices. V1 continuity is concrete encounter outcomes, routes, resources, and survivors, rather than a large narrative simulation.
- Progression includes equipment, consumables and survivor milestone levels 1–5 (issue #28). Broad character leveling, character creation, multiplayer, broad lore systems, and full tabletop coverage are outside V1.
- Save/resume must preserve the current expedition, including during combat, without restoring dead characters or rerolling previously resolved actions.

## Engineering direction

Prefer cohesive, deep native modules behind small public interfaces. The existing Unreal runtime module remains the build module. Authoritative combat/run state is separate from actors and UI; engine adapters present and submit commands. See ADR-0001, the V1 spec, and the run ledger.

The user approved the V1 brief and delegated routine architecture, testing, ticket planning, verification, and orchestration. Ask only when a discovered decision changes product intent or requires unavailable access. The technical choices and testing seams are recorded before implementation.

Issue #37 approves a later full 13-class/70-subclass compact roster and arbitrary four-slot parties. Slices #39 and #40 are implemented and accepted locally: catalog-backed builds, independent duplicate adventurers, four-slot startup/ending editing, four Barbarian paths, generic F/M controls, status/forecast display and ordered special-effect playback. Corrected source `5a5316e0` passes 55 Project tests, 50 helper tests, pinned build, Mac Development package, native keyboard/combat/Resume checks and fresh independent Standards/Spec review. The scene-culling assertion path tracked in accepted-risk #32 recurred once during native Resume; one fresh retry succeeded without changing checkpoint bytes. Pointer automation, sound and human balance remain recorded limitations. No staging integration is claimed. Next is #41, the 24-entry martial batch. Current saves use schema 4 and catalog revision 2; nine implemented kits include three interim base kits, so the full roster remains incomplete. Historical V1 scope above remains historical.
