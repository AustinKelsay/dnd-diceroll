# Ticket #4 — Standards review

Fixed baseline `c1be5672308d0d604bb08c2a1f44c688ffa2fc5f`; candidate `645cdd49697d782c9c08f626a29c78a46398fe25`. Command: `git diff c1be5672308d0d604bb08c2a1f44c688ffa2fc5f...645cdd49697d782c9c08f626a29c78a46398fe25`. Exactly one of two independent read-only gpt-6.1-sol reviewers; no editor or writes. Ticket #4 only, excluding disk persistence.

## Report

No documented-standard violations found. The candidate preserves the existing runtime build module, places authoritative run values and validation in `Run/`, keeps presentation access read-only, uses reflected value snapshots without UObject references, and tracks mesh references with `UPROPERTY()`. Tests exercise public commands/snapshots. Generated evidence remains in ignored locations.

Two optional judgment-call smells:

- **Possible Primitive Obsession — recipe definitions require coordinated indexing.** In `Private/Run/ExpeditionRun.cpp:27–39`, `Route.Recipe` selects names from `Terrain[]`; at lines 78–90, it independently selects `Patterns[]` and placement through `Route.Recipe == 1` / `== 2`. For example, `"Layout.BlockedCells = Patterns[Route.Recipe]"` and `"Terrain[Route.Recipe]"` must keep matching. A small private recipe definition containing its terrain label, blocked cells and spawn offsets would concentrate each recipe’s facts and reduce the chance that future preview changes describe a different battlefield. This does not require a general generation framework.
- **Possible Duplicated Code — survivor counting.** `Private/Run/ExpeditionRun.cpp:61–63` and `128–130` both reset `Record.Survivors` and scan the battle for `Party && Conscious` combatants. The repeated hunk is `"if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Record.Survivors; }"`. A private count query shared by encounter entry and settlement would keep the meaning of “survivors” local if life-state policies evolve.

Standards: **0 hard violations; 2 optional smells**. Indexed recipe facts are the more consequential maintenance concern.

## Owner resolution

Both optional smells are resolved by the owner: a private bounded recipe definition now owns terrain labels, obstacle cells and spawn offsets, and one private survivor-count query serves entry/settlement. No generic generation framework or expanded public interface was introduced. Fresh build and full public behavior validation cover the changes. No additional reviewers were launched.
