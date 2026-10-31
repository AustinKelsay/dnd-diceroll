# Ticket #3 — party consequences session

Sole implementation writer and editor owner on `feature/expedition-v1`. Fixed review baseline: `7c119bace4821ac04c7cf27a80e5ea9175aa8aaf`. No external writes, new checkout/branch, push, PR or tracker mutation authorized.

## Seams and decisions

Approved test seam: `FCombatBattle` Start / Submit / Snapshot / LegalTargets. Reflected values now contain typed role, life state and rescue countdown, resource counts, gear rank and potion ownership; snapshot includes the separate Bonus Action budget. `FPartyContinuity::Advance` is the atomic victory-to-next-encounter seam. It accepts a layout, seed and conscious loot recipient, carries all existing party values by stable ID, then validates a candidate battle before replacing the completed battle. Layout data can place adventurers but cannot replace their state. Dead IDs must remain in the roster. No arbitrary writable snapshot was exposed.

Rescue ticks at each downed adventurer's scheduled turn slot, three ticks to death. Rescue, Cure Wounds and potions can restore positive HP before the deadline. Victory kills unresolved downed allies; a no-conscious-rescuer wipe resolves all remaining downed to dead. These deliberate adaptations are in the rules reference and HUD. Four fixed profiles use Second Wind, qualified automatic Sneak Attack, Cure Wounds, Fire Bolt and Magic Missile. Gear/focus rank and owned potions provide progression, with no levels or implicit replenishment.

## Validation

- Read all requested project guidance, full issue #3 and prior session/review. Skills: implement, TDD, codebase-design, code-review, writing-for-agents, cua-driver. Doctor passed on UE 5.8.4 CL58902052. Initial live status at 2026-10-31 02:20:02 UTC had no project processes/listener.
- Compiled red test for Second Wind: `.local/automation/20261031T022138015105Z`. Green: `.local/automation/20261031T022240400457Z`, 1/1 passing. Compiled transition tracer failed with the stub: `.local/automation/20261031T022314110519Z`; after implementation `.local/automation/20261031T022347778162Z`, 2/2 passing.
- Additional public behavior cases cover role/target/resource rejection with full reflected-state comparison including RNG; healing caps; potion consumption; three-dart automatic damage; Sneak Attack weapon/ally/incapacitation conditions; rescue by all three means; own-slot expiry; dead-ID refusal; actual downing; no-rescuer wipe; survivor continuation; layout ownership and atomic failures; equipment damage. First expanded Expedition suite: 12/12 passing, zero warnings, `.local/automation/20261031T022520796326Z`.
- Full `Project.` validation: **17/17 passed, zero warnings**, `.local/automation/20261031T022836085666Z`; fresh Development editor build passed. Offline helpers: **22/22 passed**.
- The helper's default filter is only `Project.Starter.`; its default test run passed 1/1 in `.local/automation/20261031T022624892169Z`. Full validation explicitly uses `--filter Project.`. No default-filter result is claimed as full coverage.
- Headless editor startup automatically appended AndroidFileServer settings to DefaultEngine.ini. These unrelated generated settings are removed from the final diff.

## Composition contract for subsequent tickets

Read-only `Rules().Snapshot()` remains authoritative. Call `LegalTargets(Id, Action)` for the exact action preview and `Submit` to commit it. Existing `LegalTargets(Id)` defaults to basic Attack. Reflected `FCombatantState` carries persistent values, and `FCombatSnapshot` carries current-turn budgets and random state. No Actor or UObject reference enters the rules state.

`FPartyContinuity::Advance(Battle, Layout, Seed, RecipientId, Error)` is usable by the later run model. It requires Victory and a conscious party recipient, retains exactly the prior party identities (including dead), applies one bounded gear/potion reward, repositions from layout, rerolls initiative from the supplied encounter seed and starts a fully validated next battle. Rejected calls preserve the old battle. The runtime N-key fixture calls this same seam. Future generated encounter composition can supply its layout/seed here; run-level reward choice can evolve this narrow interface when required. Camp must be an explicit living-survivor command in that run module, and save/resume must serialize life/countdown/role/resources/equipment alongside existing combat values. Neither is implemented in this slice.
