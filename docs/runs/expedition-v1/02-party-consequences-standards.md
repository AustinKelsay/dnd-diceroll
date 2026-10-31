# Ticket #3 — Standards review

Fixed baseline `7c119bace4821ac04c7cf27a80e5ea9175aa8aaf`; candidate `56a4766`. Command: `git diff 7c119bace4821ac04c7cf27a80e5ea9175aa8aaf...56a4766`. One read-only gpt-6.1-sol reviewer, no editor access or file writes. This is one of exactly two independent review axes. Scope is ticket #3 only.

## Report

No documented-standard violations found. The slice preserves the existing Unreal module, keeps authoritative values and validation in native combat/party modules, retains reflected serializable state, uses the public command/snapshot seam for tests, and leaves UObject ownership conventions intact. Generated evidence is confined to ignored directories.

Two optional judgment-call smells:

- **Possible Duplicated Code — ability facts across rules and HUD.** `CombatBattle.cpp:205` hardcodes `GridDistance(Unit->Cell, Target.Cell) <= 6`, while `ExpeditionBattleView.cpp:318` separately embeds `"enemy, 6 cells + sight | 3d4+3 auto-hit"`. Similar range, cost and healing facts recur in `LegalTargets`, `Submit` and `ActionHelp`. These currently agree, but changing tuning requires coordinated edits and risks misleading previews. Small shared ability constants or a read-only description query would concentrate the facts without introducing a generic ability framework.
- **Possible Mysterious Name — test fixture and battle locals.** `PartyConsequencesTest.cpp:10–18` uses `FCombatEncounter E`, `FCombatantState A` and `FCombatantState B`, while subsequent tests reuse `B` for `FCombatBattle` and `R` for results. For example, `"E.Combatants = {A,B}; return E;"` obscures the roles of otherwise useful public behavior tests. Names such as `Encounter`, `Hero`, `Enemy`, `Battle` and `Result` would make failure scenarios easier to maintain.

Standards: **0 hard violations; 2 optional smells**, with duplicated ability facts the more consequential maintenance concern.

## Owner resolution

Both optional smells are resolved by the owner. `FCombatBattle::DescribeAction` now builds the action text from the same private native tuning constants used by validation and dice resolution; the controller reads that query. Test fixture and battle/result locals use domain names. No generic ability framework was introduced. Owner inspection and final build/automation cover these edits; no extra review delegation.

During owner inspection, two feedback omissions were additionally caught with compiled red tests: a last-party-member hit described a rescue window after the wipe had already made the target dead, and a finishing Sneak Attack omitted its extra-die label. Outcome resolution now precedes attack status formatting, and death plus Sneak Attack can both be reported. The same public tests pass after the fix. Qualified normal and critical Sneak Attack damage have literal worked examples; a new behavior case spends both Wind uses and a potion before a successful transition, proving only the explicit reward is added. These changes followed the independent review and were checked by the owner, not an additional reviewer.
