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

Issue #37 approves a later full 13-class/70-subclass compact roster and arbitrary four-slot parties. Slices #39 and #40 are implemented and accepted locally: catalog-backed builds, independent duplicate adventurers, four-slot startup/ending editing, four Barbarian paths, generic F/M controls, status/forecast display and ordered special-effect playback. Corrected source `5a5316e0` passes 55 Project tests, 50 helper tests, pinned build, Mac Development package, native keyboard/combat/Resume checks and fresh independent Standards/Spec review. The scene-culling assertion path tracked in accepted-risk #32 recurred once during native Resume; one fresh retry succeeded without changing checkpoint bytes. Pointer automation, sound and human balance remain recorded limitations. No staging integration is claimed. That accepted slice used schema 4/catalog revision 2 and nine implemented kits, including three interim base kits. The current martial extension is described below. Historical V1 scope above remains historical.

Issue #41 source work extends the catalog to all 24 approved Fighter/Rogue/Monk/Paladin/Ranger subclasses, retaining four Barbarian paths and interim Cleric/Wizard: 30 implemented kits across eight classes (28 approved subclasses). Thief replaces rogue.base in the familiar preset. Catalog revision 3 retains unpublished save schema 4 and the existing checkpoint path. The [martial kit reference](docs/runs/full-roster/05-martial-kits.md) records compact adaptations, costs, effect ownership, persistence and omissions. Corrected source `a1dfeb977` is accepted locally after pinned build, 63/63 Project tests (zero warnings), 50/50 helpers, fresh independent Standards/Spec review, Mac Development package and native mixed-party keyboard/combat/Resume checks. The native checkpoint was unchanged across restart/Resume and retained swapped cells, protection, spent pools and budgets. Engine risks #32/#35, automated pointer coverage, sound and human balance remain limitations. Integration is pending; slice #42 next adds all 19 approved arcane subclasses. Full70 delivery is not claimed.


Slice #42 source extends the catalog to all 19 approved Sorcerer/Warlock/Wizard subclasses: 48 implemented kits across ten classes, comprising 47 approved subclasses and interim Cleric. Evocation replaces interim Wizard; its old Magic Missile command remains a slot-and-Kit-cost compatibility adapter while the bounded action bar exposes Sculpted Burst and Focused Spark. Catalog revision 4 retains unpublished schema 4 and checkpoint discovery. See [arcane kits](docs/runs/full-roster/07-arcane-kits.md) for exact compact adaptations, shared resources, surge ownership and omissions. Source `759f8e299` is accepted locally after pinned build, 67/67 Project tests (zero warnings), 50/50 helpers, fresh independent Standards/Spec reviews, Mac Development package and native mixed-party keyboard/combat/fresh-process Resume. Committed surge, protection, swapped cells, spent pools/budgets and checkpoint bytes survived Resume. Evidence/limitations are in the ledger; engine risks #32/#35 remain unresolved. Next #43 adds all19 Cleric/Druid/Bard subclasses. Full70 and staging integration are not claimed.
