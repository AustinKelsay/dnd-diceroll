# Expedition V1

## Problem Statement

The player wants attachment to a consistent party while experiencing unpredictable fantasy expeditions with readable turn-based tactics, consequential decisions, and permanent losses.

## Solution

One player controls four prebuilt adventurers through three generated encounters including a finale. They choose routes, manage resources, find equipment/consumables, use one recovery opportunity, and either recover the objective or lose their party. Original fantasy content uses an explicit SRD 5.2.1 subset.

## User Stories

1. As a player, I want a fresh four-role party so I can start without character creation.
2. As a player, I want stable adventurer identities so I can follow survivors through a run.
3. As a player, I want a rotatable overhead view so I can inspect the battlefield.
4. As a player, I want to select any party member and see whose turn it is.
5. As a player, I want reachable positions and valid targets shown before committing.
6. As a player, I want movement limited by terrain, occupancy, and the current turn.
7. As a player, I want clear attack, defense, damage, and dice results.
8. As a player, I want distinct fighter, rogue, cleric, and wizard abilities.
9. As a player, I want ability resources and consumables to matter across fights.
10. As a player, I want enemies to take legal, understandable turns without hanging.
11. As a player, I want allies at zero health to have a limited rescue opportunity.
12. As a player, I want actual deaths to remain permanent for the expedition.
13. As a player, I want a smaller surviving party to continue.
14. As a player, I want a party wipe to produce a clear run-ending result.
15. As a player, I want route options to communicate partial risk/reward information.
16. As a player, I want generated terrain, enemy groups, and placement to vary.
17. As a player, I want previous choices and encounter outcomes to affect the run.
18. As a player, I want equipment/consumable rewards to help survivors progress.
19. As a player, I want one bounded recovery opportunity that never resurrects the dead.
20. As a player, I want the third encounter to conclude the simple artifact objective.
21. As a player, I want save/resume to preserve the current run, including combat.
22. As a player, I want saved deaths and resolved random outcomes preserved on resume.
23. As a player, I want completed-run records without permanent combat stat bonuses.
24. As a player, I want a new run to reset party power and resources.
25. As a player, I want input instructions, action feedback, and run status visible.
26. As a maintainer, I want reproducible seeds and stable state interfaces so behavior can be tested and extended.

## Implementation Decisions

- Keep the native runtime build module. Separate cohesive combat/run rules from Unreal actors, UI, camera, input, and save adapters (ADR-0001).
- Public command/result/snapshot interfaces are authoritative. Validate turn ownership, targets, resources, phase, occupancy, and ranges before mutation. Rejected commands leave state and random progression unchanged.
- Content definitions identify supported roles, attacks/abilities, enemies, encounter recipes, and rewards. Avoid broad frameworks for hypothetical future features.
- Deterministic random state belongs to the rules/run state and is persisted. Same seed and command sequence produce the same results; separate generation and combat streams if useful.
- Stable combatant/party identities survive presentation rebuilds and encounters. Downed and dead are distinct states. Dead members cannot act, recover, or receive resurrection.
- Three encounters, route selection between them, one recovery allowance, and victory/defeat terminals. Initial goal is artifact recovery at the finale. No leveling in V1.
- Modular layouts (16–18 × 11–12 cells since [#29](https://github.com/AustinKelsay/expedition-rpg/issues/29); originally 8–9 × 7–8) must have valid cells, nonoverlapping walkable spawns, and routes to actionable targets. Difficulty/reward variation uses bounded recipes and persists the chosen opportunity.
- Save/resume uses a versioned validated snapshot, including phase, party, encounters, RNG, turn, resources, deaths, and run identity. Fail clearly on invalid/incompatible saves; avoid corrupting the last valid save on write failure. Completed run records grant no power.
- Use a controllable 3D overhead camera and readable simple visuals with visible selection, turn, health, status, range, action feedback, and controls.
- Actor spawning, UI refreshes, and repeated input cannot bypass validation, spend a resource twice, or apply duplicate outcomes.
- Original-world content and a documented supported rules subset; exact numeric balancing is delegated engineering/game-design tuning. Any departures from the SRD are labeled adaptations.

## Testing Decisions

The user explicitly delegated routine architecture, testing, verification, and ticket planning after approving the V1 scope. These technical seams are therefore selected under that authorization, without repeating the completed product interview.

- Test combat behavior through the public command/result/snapshot interface: legal and illegal movement, range/line-of-sight as supported, initiative/turns, hit/miss/critical, resource use, downing/rescue/death, enemy turns, and battle terminals. Catches rules errors; does not establish rendering/input.
- Test expedition behavior through the public run interface: seeded generation, route selection, persistent survivors/resources/rewards, one-use recovery, finale victory, wipe defeat, and fresh-run reset. Catches transition/data ownership errors; does not establish camera or UI.
- Round-trip real save serialization through the public persistence seam and continue the same commands after restore. Exercise invalid/truncated/wrong-version snapshots and preserve the last valid save. Catches lost state/RNG and invalid restores; does not establish OS or power-loss guarantees beyond the adapter's contract.
- Use existing Unreal native automation conventions under Project.Expedition. Tests specify observable behavior, not private collaborators, and deterministic worked examples control randomness at the seam when required. Use red→green cycles per behavior.
- Compile each slice with editor closed. Observe actual PIE, mouse/keyboard selection and targeting, camera, HUD, win/defeat, and save/resume through real rendered output. Capture and inspect evidence; API state alone is insufficient for interaction claims.
- Final acceptance: helper tests, scoped engine automation, fresh bootstrap, current-project identity, rendered PIE interaction, screenshots, Mac Development package and visible packaged startup/play observation. Record exact evidence and limitations.

## Out of Scope

Character creation, leveling, multiplayer, official D&D lore/setting IP, full SRD coverage, runtime AI narration, recruitment, resurrection, permanent power grind, open world, elaborate factions/story simulation, production distribution/signing, and non-Mac targets.

## Further Notes

Working title: Expedition. The reusable boilerplate repository stays unchanged. Implementation uses one feature branch and reviewed per-ticket commits, with a non-draft PR targeting staging. Product/design contradictions and unavailable privileged access are the remaining human escalation triggers.
