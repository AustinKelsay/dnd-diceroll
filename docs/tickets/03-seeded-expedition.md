# Complete a seeded expedition

## Parent and dependency

[Expedition V1](https://github.com/AustinKelsay/expedition-rpg/issues/1). Blocked by: Survive losses and use party roles.

## Outcome

A fresh party plays a complete three-encounter artifact expedition with route decisions, generated battlefields, rewards, one recovery opportunity, and a clear ending.

## Acceptance criteria

- New runs begin with fresh party power/resources and a visible seed/run identity.
- Route options disclose partial risk/reward information; selected opportunities and outcomes remain part of authoritative state.
- Three combats, including a finale, vary bounded terrain/layouts, enemy groups and placement, objectives or rewards while guaranteeing valid spawns and traversable actionable spaces.
- Surviving adventurers, deaths, health, resources, equipment and consumables persist. One recovery opportunity cannot be repeated or resurrect anyone.
- Final objective victory and party-wipe defeat terminate the run clearly. Restart resets combat power rather than granting permanent bonuses.
- Deterministic behavior tests cover seed reproducibility, generation validity, route constraints/history, rewards, one-use recovery, finale completion, loss and fresh-run reset.
- Compile and play an actual rendered route → combat → reward/recovery → finale flow with visible state and controls. Review and commit the slice with evidence.

## Scope

Keep the story to a simple artifact objective and concrete recorded outcomes. No open world, procedural prose service, complex factions, leveling or custom party creation.

Tracker: https://github.com/AustinKelsay/expedition-rpg/issues/4
