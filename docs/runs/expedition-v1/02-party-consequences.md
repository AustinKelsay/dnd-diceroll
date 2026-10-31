# Ticket #3 — party consequences session

Sole implementation writer and editor owner on `feature/expedition-v1`. Fixed review baseline: `7c119bace4821ac04c7cf27a80e5ea9175aa8aaf`. No external writes, new checkout/branch, push, PR or tracker mutation authorized.

Implementation commits: `56a4766d79d63206e2788957270ee5b8b2337ac5` and `6cc0eb2d331e6361ae9b33c226a69fd7285a7501`. This handoff is a documentation-only follow-up.

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

## Review and final owner checks

Exactly two independent read-only gpt-6.1-sol reviewers inspected `git diff 7c119bace4821ac04c7cf27a80e5ea9175aa8aaf...56a4766`. [Standards](02-party-consequences-standards.md): zero hard violations, two optional maintainability smells, both owner-resolved. [Spec](02-party-consequences-spec.md): zero code findings, native evidence then pending and subsequently completed below. No extra reviewers were started.

Owner follow-up moved descriptions/tuning into the combat module and clarified test locals. Public red regressions caught two feedback omissions (terminal wipe wording and missing Sneak Attack label on a finishing hit): `.local/automation/20261031T025737535866Z`, exactly those two expected assertion failures. Corrected formatting, literal normal/critical Sneak Attack damage examples, and actual spent Wind/potion carry-over pass in the final suite. Fresh editor build passed; **18/18 Project tests, zero warnings**, `.local/automation/20261031T025850681275Z`, and **22/22 helper tests** passed. The final test-only comparison-parameter naming cleanup is covered by the later final receipt below.

## Composition contract for subsequent tickets

Read-only `Rules().Snapshot()` remains authoritative. Call `LegalTargets(Id, Action)` for the exact action preview and `Submit` to commit it. Existing `LegalTargets(Id)` defaults to basic Attack. Reflected `FCombatantState` carries persistent values, and `FCombatSnapshot` carries current-turn budgets and random state. No Actor or UObject reference enters the rules state.

`FCombatBattle::DescribeAction(Action)` returns the player description using the same tuning as validation/resolution.

`FPartyContinuity::Advance(Battle, Layout, Seed, RecipientId, Error)` is usable by the later run model. It requires Victory and a conscious party recipient, retains exactly the prior party identities (including dead), applies one bounded gear/potion reward, repositions from layout, rerolls initiative from the supplied encounter seed and starts a fully validated next battle. Rejected calls preserve the old battle. The runtime N-key fixture calls this same seam. Future generated encounter composition can supply its layout/seed here; run-level reward choice can evolve this narrow interface when required. Camp must be an explicit living-survivor command in that run module, and save/resume must serialize life/countdown/role/resources/equipment alongside existing combat values. Neither is implemented in this slice.

## Native play evidence

Exact project/engine identity was verified as editor PID 94568, UE `5.8.4-58902052+++UE5+Release-5.8`, canonical project path and sandbox, with no dirty packages. Discovered/described `EditorAppToolset` and started a floating PIE session. CUA's background key attempt was refused for same-PID editor/PIE ambiguity; the documented exact-window foreground fallback delivered real key input. All player actions below used keyboard controls, with window snapshots around input and inspected PNGs. No direct rules-state manipulation or console cheats drove the playthrough.

- Cleric Cure Wounds healed Ivo 19→22, spent one slot and the Action: `.local/captures/party-cure-2-enter.png`.
- Rogue qualified Sneak Attack hit Briar for 9, HP 16→7, with `SNEAK +d6`: `.local/captures/party-cure-6-enter.png`.
- Wizard Magic Missile spent a slot and automatically dealt 12 to Ash, HP 16→4: `.local/captures/party-missile-6-enter.png`. The second cast spent the final slot and killed Ash: `.local/captures/party-rescue-10-enter.png`.
- Ivo became Downed at 0 HP with three ticks visible: `.local/captures/party-down-3-space.png`. R + adjacent target + Enter restored him to 1 HP and spent Sera's Action: `.local/captures/party-rescue-2-enter.png`.
- Nix drank his own potion, healed the capped missing 2 HP and consumed the bottle, 1→0: `.local/captures/party-clear-raider-6-enter.png`.
- Ivo was downed again. Subsequent own-slot countdown states 3→2→1 and eventual `DEAD - permanent` were inspected in `party-clear-raider-9-space.png`, `party-advance-survivors-14-space.png`, `party-second-wind-setup-6-space.png` and `party-second-wind-8-space.png` under `.local/captures/`.
- Fighter Second Wind healed Arden 23→30, uses 2→1; Bonus SPENT while Action remained READY: `.local/captures/party-second-wind-7-enter.png`.
- The three survivors finished the battle. Arden moved three cells and killed the final archer for 6 damage; Victory and permanent Ivo loss were visible: `.local/captures/party-finish-7-enter.png`. This supplies native victory evidence beyond the previous slice's automated-only terminal coverage.
- Selected dead Ivo and pressed N: rejected. Selected Nix and pressed N: next encounter began with Ivo still dead and omitted from initiative, Arden still 28/30 HP with one Wind use, Sera still one slot, and Nix's spent potion remaining spent except for the single reward bottle (0→1) and gear rank (0→1). Repeated N was rejected without another reward. Images: `.local/captures/party-next-1-n.png`, `party-next-3-n.png`, `party-next-4-n.png`.
- Discovered/described `CaptureEditorImage` captured the actual editor/PIE render, and both PNGs were inspected: `.local/captures/party-victory-editor.png` and `.local/captures/party-continuation-editor.png`. The detailed CUA window images above have larger readable game text. Standard helper capture intentionally refuses PIE.
- PIE stopped through MCP. At 2026-10-31 02:56:59 UTC, helper status verified PID 94568, correct engine/path, `pie_active: false`, `dirty_packages: []`. Normal helper close then exited that exact PID. Source review fixes began only after closure. No binary asset changes or helper rewrites.

Limitations: this slice uses the allowed explicit repeated-crossing fixture, not generated routes/three encounters/camp/save/resume. Native victory, all four role features, rescue, expiry/death and continuation were observed; full-party wipe is automation-verified. No package was repeated at this intermediate slice. The prior packaged mouse coordinate-delivery gap remains; this session used the verified keyboard path.


## Final validation and editor handback

Fresh editor PID 35478 verified the canonical project and pinned engine. Smoke at **2026-10-31 03:01:37 UTC** started/stopped PIE, had no dirty packages and **zero errors**, with one host AudioUnit sample-rate warning (`2003332927`); `.local/party-final-smoke.log`. Final shared default action text and render were inspected in `.local/captures/party-final-shared-description.png` and `.local/captures/party-final-editor.png`. A single post-relaunch F probe did not visibly switch modes, so this final image establishes the shared default description rather than a second role-use replay; complete native role input evidence is the earlier playthrough above.

PIE was stopped through the discovered tool. At **2026-10-31 03:04:15 UTC**, `.local/party-final-before-close.json` verified PID 35478, exact engine/path, `pie_active: false` and `dirty_packages: []`. Normal helper close completed (`.local/party-final-close.log`). A final test-only rename clarified snapshot-comparison parameters after closure; no runtime behavior changed. Final fresh build passed (`.local/party-handoff-build.log`), **18/18 Project tests passed with zero warnings** (`.local/automation/20261031T030513495054Z`), and **22/22 helper tests passed** (`.local/party-handoff-helper-tests.log`). These tests include the preserved sandbox asset and map PIE checks. No asset fixture changed, so no bootstrap repair was needed.

Final live status at **2026-10-31 03:05:41 UTC**: **no associated project processes and no MCP listener**, `.local/party-final-status.json`. Editor ownership returns to root. Other project browser was untouched. No binary asset changes, helper rewrites, branch/checkout creation, push, PR, issue mutation or external messages were performed. Generated evidence remains ignored. The package and packaged mouse limits stated above remain for root's final acceptance.
