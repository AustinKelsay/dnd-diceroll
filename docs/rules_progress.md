# Rules progress — spell handoff (2026-11-02)

Branch `feature/dnd-defaults`. Last commit: 7a6c533 (utility spell resolution). Suite: 238 automation tests green.

## Spell coverage (SRD 5.2.1, castable / total / exceptions)
| Level | Castable | Total | Exceptions |
|---|---|---|---|
| 0 | 19 | 27 | 8 |
| 1 | 39 | 57 | 18 |
| 2 | 39 | 57 | 18 |
| 3 | 22 | 42 | 20 |
| 4 | 24 | 34 | 10 |

SpellCoverageTest floors: {19, 39, 39, 22, 24}. Exceptions are rows in SpellUtilityRules.cpp (Kind=Exception), each with its reason.

## Done in 7a6c533
- Beams (Eldritch Blast / Scorching Ray / Magic Missile split via ExtraTargetIds), Chromatic Orb / Sorcerous Burst (Command.Choice damage type, leaps), Ice Knife, True Strike, Shillelagh.
- Spare the Dying, Revivify, Lesser Restoration, Dispel Magic; Misty Step / Dimension Door (Command.Destination); Gust of Wind push.
- Zones: Fog Cloud, Darkness (cancels light), Light family, Silence, Plant Growth (4x move cost), Guardian of Faith, Aura of Life, CME, Pass without Trace.
- Summons turn-start attacks, Hex, Warding Bond, Resilient Sphere, Confusion/turn control.
- FProficiencyRules::PrebuiltCharacter no longer asserts for classes without a prebuilt skill choice (falls back to prebuilt scores) — needed for saves by catalog-build heroes.

## Remaining (next steps)
1. Upcast tests with 4th-level slots (damage dice / extra targets / beams scale).
2. VFX hook API, no art: SizeClass, Persist, public SpellVisuals() on FCombatBattle.
3. docs/rules-reference.md: document the utility kinds and per-spell rulings.
