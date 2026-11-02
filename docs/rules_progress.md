# Rules progress: spell handoff (2026-11-02)

Branch `feature/dnd-defaults`. Suite: 241 automation tests green. No push.

## Commits this phase
- 7b675d3: exception re-audit. Minor/Silent/Major Image become sight-blocking zones, Remove Curse ends Hex and Bestow Curse, and Blink gives Ethereal on a d6 roll of 4-6. Coverage floors raised.
- 00a3603: upcast tests (`Spells.UpcastRules`, `Spells.UpcastBattle`).
- 3929159: VFX hook API. SizeClass tiers 0 / 1-2 / 3 / 4-6 / 7-9, `ESpellEventKind::Persist`, `FSpellCastEvent::SizeClass` and `bPersist`, `FCombatBattle::SpellVisuals()`, the `OnSpellEvent` hook, and the `Spells.VisualHooks` test.
- (this commit): docs. Utility kinds, per-spell rulings, exception reasons and status tables in rules-reference.md.

## Spell coverage (SRD 5.2.1, castable / total / documented exceptions)
| Level | Castable | Total | Exceptions |
|---|---|---|---|
| 0 | 20 | 27 | 7 |
| 1 | 40 | 57 | 17 |
| 2 | 39 | 57 | 18 |
| 3 | 25 | 42 | 17 |
| 4 | 24 | 34 | 10 |

SpellCoverageTest floors: {20, 40, 39, 25, 24}. Exceptions are the Kind=Exception rows in SpellUtilityRules.cpp, each with its reason, listed in rules-reference.md.

## Remaining gaps
- No live 4th-level cast: EngineLevelCap = 5, and Start rejects a 4th-level slot. 4th-level scaling is tested in the rules library only.
- Still exceptions: exploration, social and ritual spells, plus Polymorph, Wind Wall, Gaseous Form, Alter Self, Dragon's Breath and Goodberry (reasons in the table).
- The VFX hook has no art and no Blueprint/UObject delegate wrapper yet. `OnSpellEvent` is a C++ `TFunction`.
