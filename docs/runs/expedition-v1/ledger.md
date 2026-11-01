# Expedition V1 — Feature Dev Ledger

This is the completed V1 delivery record. Continue from the [presentation-polish handoff](../expedition-polish/01-presentation-polish.md) and the current workflow in [development.md](../../development.md). The launcher named below was retired after this run; historical validation and review records remain unchanged.

## Run

- Run ID: expedition-v1-20261030
- Loop: plebdev-feature-dev (validated v0.4.0)
- Target repo: AustinKelsay/dnd-diceroll (private; derived from the unchanged unreal-ai-boilerplate baseline)
- Base: staging, created at ea2a3c0047064668a018e5479546d7884dc12b83
- Feature branch: feature/expedition-v1
- Owner: AustinKelsay; orchestrator owns the branch and editor ownership transfers
- Status: delivery complete; staging PR #6 open and non-draft; local gates and independent PR fallback review complete
- Skill setup: GitHub issues, canonical labels, single-context domain docs
- Models: implementation/support/review workers use gpt-6.1-sol as explicitly requested; inherited high reasoning

## Goal

Deliver the approved party-based 3D tactical roguelike V1, focusing on reusable, scalable foundations, tests along the way, structured documentation and reviewed PR delivery. The user explicitly asked the orchestrator to lead end to end and surface questions only when needed.

## Decisions and authority

The product interview and V1 brief are approved. The user delegated routine engineering, testing, documentation, ticket granularity, and orchestration decisions. This overrides repeat confirmation prompts for setup, technical test seams, and ticket dependency planning in the launcher defaults. Scope-changing intent, irreversible product choices, and unavailable privileged access still escalate. No further product question is presently blocking.

Use GitHub, staging, one feature branch, per-ticket commits, public-interface tests, two-axis code review, local CodeRabbit and bounded PR review/fallback. No production operations. No merge to main is authorized by this pipeline's completion condition.

## Durable artifacts

- Approved brief: docs/game-brief.md
- Context/glossary: CONTEXT.md and GLOSSARY.md
- Architecture: docs/architecture.md and ADR-0001
- Spec: docs/specs/expedition-v1.md — https://github.com/AustinKelsay/dnd-diceroll/issues/1
- Tickets: docs/tickets/ — published with native parent/dependency links
- Session/review evidence: docs/runs/expedition-v1/
- Local machine evidence: ignored .local/ and Builds/
- PR: https://github.com/AustinKelsay/dnd-diceroll/pull/6 (open, non-draft, base staging)

## Commands

- Host: /opt/homebrew/bin/python3 tools/dev.py doctor
- Helper tests: /opt/homebrew/bin/python3 -m unittest discover -s tools/tests
- Compile: /opt/homebrew/bin/python3 tools/dev.py build (editor closed)
- Asset fixture: /opt/homebrew/bin/python3 tools/dev.py bootstrap
- Engine tests: /opt/homebrew/bin/python3 tools/dev.py test --filter Project.Expedition.
- Editor: status → close → build → editor; one owner, sequential MCP
- Observation: PIE, real interaction, capture and inspected PNG; tools/dev.py smoke remains the starter fixture check
- Packaging: /opt/homebrew/bin/python3 tools/dev.py package plus visible packaged verification
- Review: coderabbit review --agent --type all --base staging, or recorded fresh Sol review fallback

## Ticket ledger

| Slice | Blocked by | Owner/status | Evidence |
| --- | --- | --- | --- |
| Play a tactical battle | none | implementation complete; commits `e8b21f5`, `92b6a83`; root resumes orchestration | final build; 9/9 Project tests; 22/22 helper tests; Mac package + native keyboard play; separate Standards/Spec reviews resolved; [session](01-tactical-battle.md) and [review](01-tactical-battle-review.md) |
| Survive losses and use party roles | battle | implementation complete; `56a4766`, `6cc0eb2`; root owns source/editor and external delivery | [session](02-party-consequences.md); 18/18 Project tests, 22/22 helpers; native four-role use, rescue/death, victory and smaller-party continuation; separate [Standards](02-party-consequences-standards.md) and [Spec](02-party-consequences-spec.md) reviews resolved |
| Complete a seeded expedition | losses/roles | implementation complete; `645cdd4`, `a13fafa`; source/editor returned to root | [session](03-seeded-expedition.md), separate [Standards](03-seeded-expedition-standards.md) and [Spec](03-seeded-expedition-spec.md); final fresh build, 26/26 Project, 22/22 helpers; native three-combat artifact ending and fresh reset |
| Resume a run and record its ending | expedition | implementation complete; `accd4c0`, `812366a`; source/editor returned to root | [session](04-persistence-records.md), separate [Standards](04-persistence-records-standards.md) and [Spec](04-persistence-records-spec.md) resolved; 34/34 Project, 22/22 helpers, preserved bootstrap, Mac package, exact rendered fresh-process resume, native wipe/history/reset and final PIE ending record |

## Editor ownership

Original editor PID 80712 was verified clean and exited normally through the helper before game changes. One implementation worker at a time may build or operate the editor. Read-only support never calls MCP, launches, or closes Unreal. Root confirmed the close; the next implementation worker owns the editor until it reports completion.

## Escalations and parked work

None. Agent-performable local/visual/package checks are AFK. Other platforms and production distribution are out of scope. Aesthetic tuning can follow the verified simple presentation without blocking the first playable.

## Setup evidence

2026-10-30: created the private game repository; baseline main/staging pushed, original remote retained as boilerplate. Published spec #1 and tickets #2–#5 with native parent/dependency relationships. Doctor passed and all 22 helper tests passed. Setup commit: 31a48fb0342013de151461113fc88be727c5d686. No PR yet.

## Tracker

- Play a tactical battle: https://github.com/AustinKelsay/dnd-diceroll/issues/2
- Survive losses and use party roles: https://github.com/AustinKelsay/dnd-diceroll/issues/3
- Complete a seeded expedition: https://github.com/AustinKelsay/dnd-diceroll/issues/4
- Resume a run and record its ending: https://github.com/AustinKelsay/dnd-diceroll/issues/5

## Tactical battle handoff — 2026-10-31

Implementation commits: `e8b21f5` (initial battle slice), `92b6a83` (review fixes, regression, final native input paths). Fixed review baseline `bca18be64b76ab7c0cac1cae71915ddccad489b2`; exactly two independent read-only gpt-6.1-sol axes. Standards: no hard violations, two optional duplication findings resolved. Spec: one P2 rejected-input starvation issue resolved with red/green adapter regression. Owner inspected the later small keyboard-grid addition and ran final validation.

Evidence: `.local/automation/20261031T020825536544Z` has 9 passing Project tests and no warnings. All 22 helper tests pass. `Builds/20261031T020904710358Z` has successful Mac Development cook/archive and exact-map headless startup; the same app was visibly played through round 2 with real keyboard movement, blocked-target rejection, legal AC preview, attack roll/HP/action changes, turn progression, AI, camera rotation and wheel zoom. Earlier native PIE mouse hit is captured in `.local/captures/after-desktop-q-before.png`; final editor image is `.local/captures/final-battle-pie.png`. Packaged mouse coordinate delivery via CUA remained unreliable; the complete arrows/Enter input path is verified. See session for exact limits and public interface. No binary asset edits, helper rewrites, pushes, PRs or tracker state changes.

Editor ownership handback complete: exact-project PID 83142 was verified clean with PIE stopped, then exited normally through the helper. Final status at 2026-10-31 02:16:55 UTC showed no associated project processes and no MCP listener; no packaged game remained. Other project browser untouched. Final smoke had zero errors and one host AudioUnit sample-rate warning (`2003332927`). Root now owns editor coordination, the next slice and all external delivery operations.

## Root acceptance and next handoff

Root inspected the final PIE image and the separate review/session evidence. Ticket #2's battle scope is accepted locally; packaged mouse targeting and native terminal play remain explicit final acceptance gaps. Victory/defeat are presently automation-verified. Primary-source SRD findings are retained in `docs/srd-source-notes.md` for ticket #3; actual supported rules stay in `docs/rules-reference.md`. The next worker owns source/editor operations and must return verified ownership when done. No remote feature push or PR yet.


## Party consequences handoff — 2026-10-31

Ticket #3 implementation complete locally. Commits: `56a4766d79d63206e2788957270ee5b8b2337ac5` (roles/losses/continuation), `6cc0eb2d331e6361ae9b33c226a69fd7285a7501` (owner-resolved review smells, feedback regressions, final evidence). Review baseline `7c119bace4821ac04c7cf27a80e5ea9175aa8aaf`; exactly two independent read-only gpt-6.1-sol reviews against candidate `56a4766`. Standards: zero hard violations, two optional smells, both resolved. Spec: zero code findings; pending native evidence completed. Later owner changes had red/green public feedback regressions and final full validation; no extra reviewers.

Final fresh editor build passed. `.local/automation/20261031T030513495054Z`: **18/18 Project tests, zero warnings**. Offline helpers: **22/22 passed**. Native keyboard PIE used all four role features, potion consumption, downing, R rescue, own-slot deadline expiry to permanent death, victory, dead-recipient rejection, and next-encounter loot/continuation with only three surviving turns. Detailed inspected captures are in the session. `.local/captures/party-next-3-n.png` shows dead Ivo retained, Arden still 28/30 HP with one Wind use, Sera still one slot, Nix receiving exactly one gear rank and potion. Repeated N rejected without another reward. No map or binary asset changes.

Public seams: `FCombatBattle` retains command/snapshot ownership, now with typed roles/life/countdown, resources/equipment/consumables and a Bonus Action budget. `LegalTargets(Id, Action)` previews the same validation as `Submit`; `DescribeAction(Action)` exposes shared tuning text. `FPartyContinuity::Advance(Battle, Layout, Seed, RecipientId, Error)` atomically carries the exact party roster and persistent values into a validated next encounter and awards bounded loot once. The allowed repeated-crossing fixture is wired to N. Generated routes/three encounters, camp and save/resume remain later tickets; see session composition contract.

Editor ownership returned: final exact-project PID 35478 had PIE stopped and no dirty packages at **2026-10-31 03:04:15 UTC**, then exited normally through the helper. Final status **2026-10-31 03:05:41 UTC** (`.local/party-final-status.json`) showed **no associated project processes or MCP listener**. Other project browser untouched. Final fresh smoke: zero errors, one known host AudioUnit sample-rate warning (`2003332927`). Root owns subsequent source/editor operations, independent full-feature review and tracker/PR delivery. No remote writes were performed.

Limits: native victory is now observed; wipe remains automation-verified. No intermediate package repeat was required; root's final package acceptance and prior packaged mouse coordinate-delivery gap remain. Native interaction here used the reliable established keyboard session; a final post-relaunch single F probe was a no-op, so the last render is not claimed as a second role replay. The full earlier role/rescue/loss/continuation playthrough remains the native evidence.

## Root party acceptance and expedition handoff

Root inspected the survivor-continuation image, public composition seam and separate review evidence. Ticket #3 accepted and closed after validation; parent spec remains open. Root live status at 2026-10-31 03:09:52 UTC confirms no associated project process or MCP listener. Next worker owns source/editor operations for ticket #4; later save/record work remains pending. The old repeated-crossing fixture should now be replaced by the authoritative three-encounter run flow. Final package/mouse and native defeat acceptance gaps remain explicit.


## Seeded expedition handoff — 2026-10-31

Ticket #4 implementation complete locally. Fixed review baseline `c1be5672308d0d604bb08c2a1f44c688ffa2fc5f`; implementation `645cdd49697d782c9c08f626a29c78a46398fe25`, owner follow-up `a13fafaa8a11d804a0e2915ccabfde9df0b000ea`. Exactly two independent read-only gpt-6.1-sol axes: Standards zero hard violations/two optional smells (both owner-resolved); Spec zero code findings/native composition pending at candidate (now completed). Owner also caught finale potion metadata with a red public assertion and fixed it. Separate reports and exact evidence are linked in the ticket table.

Final fresh editor build succeeded; `.local/automation/20261031T035207894828Z`: **26/26 full Project tests, zero warnings**. Helpers: **22/22**. Preserved sandbox bootstrap: zero errors/warnings. Public tests cover 128 generation seeds, occupied-spawn access/connectivity, whole-run replay across 16 seeds, atomic rejection/RNG, history effects, exact rewards/camp/no resurrection, smaller-party continuation, three-combat artifact victory, wipe and fresh reset. Fresh editor smoke had zero errors and one known host AudioUnit sample-rate warning (`2003332927`).

Real keyboard native play completed route → first victory → reward → explicit camp → changed layout → second victory/reward → finale → **ARTIFACT RECOVERED (3/3)** → **N fresh seed/reset**. Primary inspected captures: `.local/captures/run-final-sera-6-return.png` and `.local/captures/run-fresh-0-n.png`. Final code framing capture `.local/captures/run-final-framing.png` verifies all eight rows above the bottom HUD. Final post-relaunch input probes were no-ops and are not claimed as a second replay; the earlier complete native session is the composition evidence. Mouse delivery remains unreliable, native wipe/smaller-party run continuation are automation-only this slice, and root retains final package/packaged-play acceptance.

`FExpeditionRun` now owns the sole battle and run phase/metadata. Public validated commands are NewRun, ChooseRoute, ClaimReward, Recover, Submit and PlayEnemyTurn; Battle/Progress are const and Snapshot assembles reflected run+battle values with retained generation/combat RNG, offers/history/camp/party/phase. Ticket #5 must add versioned validated atomic restore, preserving private initialization and cross-field invariants rather than restarting/rolling state. No disk adapter is present. See the session serialization contract before implementing it.

Editor ownership returned: exact project PID **89957** was verified PIE stopped/dirty packages empty at **2026-10-31 03:57:40 UTC**, then exited normally through the safe helper. Final status **03:57:45 UTC** (`.local/run-final-status.json`) showed **no associated project processes or MCP listener**. CUA session ended; other project browser untouched. Root now owns source/editor coordination, final integrity review, ticket #5 and all external delivery. No remote writes or binary asset changes were performed.

## Root expedition acceptance and persistence handoff

Root inspected the artifact-ending capture, public snapshot/run composition contract and independent reviews. Ticket #4 accepted and closed after validation. Root live status at 2026-10-31 04:01:29 UTC confirms no associated project process or MCP listener. Next worker owns source/editor operations for ticket #5. Final save/record integrity, packaged rendered save/resume and full-feature review remain required; the mouse automation and native wipe limitations stay explicit. Parent spec and final PR delivery remain root-owned.

## Persistence handoff — 2026-10-31

Ticket #5 is implementation-complete locally. Fixed review baseline `bf804da5ed4d8d5e892525d0d42a5a390a701c86`; implementation `accd4c070199abbc667cb50d4960cbf6a8fc80e4`, owner review/control fixes `812366a3b8ff1c53f1c83162948a9830aa2a3f65`. Exactly two independent read-only gpt-6.1-sol reviews: Standards zero hard violations/one optional duplication smell; Spec two P2 findings (contradictory retained route descriptions and inaccessible older records). All resolved by the owner, with public rejection/paging tests and full regression checks. No extra reviewers.

Public contracts: validated candidate `FCombatBattle::Restore` / `FExpeditionRun::Restore`, strict version-1 `FExpeditionSave`, and command/checkpoint-owning `FExpeditionSession`. Accepted gameplay writes before installation, so checkpoint failure rejects the entire candidate including RNG/deaths. Exact active/between-encounter state resumes without rolls. Current checkpoint and deduplicated ending records share one atomic Mac replacement; fresh same-seed runs get distinct identities. Records grant no power. Startup is read-only; N/O choose fresh/resume, K saves, H inspects records and PgUp/PgDn pages them. Architecture/rules/session record storage paths and guarantees.

Final fresh build succeeded. `.local/automation/20261031T042811060418Z`: **34/34 full Project tests, zero warnings** (original 26 retained). Helpers **22/22**. Preserved sandbox bootstrap zero errors/warnings. `Builds/20261031T042837285648Z`: Mac Development cook/archive/exact-map startup passed. Real packaged PID 29575 saved Ivo's spent Action, Move 1, one remaining slot, injured enemy and RNG; normal quit and fresh PID 33328/O retained the entire checkpoint exactly. Default storage was separately verified. Real packaged PID 34348 then reached permanent deaths and **PARTY WIPE**, reloaded the ending, inspected its single history record, and started a full fresh party while retaining the record. Final PIE PID 42011 loaded that actual packaged ending and displayed history; native and MCP captures were inspected. The previous native-wipe gap is closed. See the session for exact evidence and owned-fixture cleanup.

Editor handback: PID 42011 was verified PIE stopped and dirty packages empty at **04:54:15 UTC**, then closed normally through the exact-identity helper. **04:54:55 UTC** final status shows no associated project process/listener, and no owned game app remains. CUA session ended; unrelated project/browser untouched. No binary asset changes or remote mutations. Root owns source/editor operations, its separate fresh full-feature integrity review, final CodeRabbit, tracker acceptance and non-draft PR. Mac-only durability bounds and CUA mouse limitations remain explicit; all requested local gameplay/persistence work is complete.

## Root final local gates

Independent full-feature integrity review completed with no actionable findings at 83cfb63. Local CodeRabbit completed with two valid startup issues; root fixed them with compiled failing/passing public regressions, final 36/36 full Project tests and 22/22 helpers. Final Mac Development package is Builds/20261031T050537650861Z; native startup protection and unreadable-save preservation verified. See 05-integrity-review.md, 06-coderabbit-local.md and 07-root-delivery.md. Root status at 05:09:08 UTC: no editor/listener or owned packaged game.

## Final PR acceptance — 2026-10-31

PR #6 is open and non-draft against staging. The CodeRabbit request at 05:11:25 UTC remained silent at 05:42:44 UTC, beyond the pipeline's 20-minute cap. A fresh independent read-only gpt-6.1-sol fallback reviewed the complete diff from ea2a3c0 to 95adf5f and found **no actionable correctness, spec or maintainability issues**. It confirmed a clean working tree and passing diff whitespace check; it read rather than reran the recorded execution evidence. See [08-pr-review.md](08-pr-review.md).

No hosted checks or GitHub approvals were reported. Local test/package/native evidence and the completed fallback satisfy this delivery pipeline; a future merge remains a separate decision. Parent spec #1 stays open; children #2–#5 are closed. No product/access questions or unresolved review findings remain. Root returns the open PR and recorded limitations to the user without merging or deploying.
