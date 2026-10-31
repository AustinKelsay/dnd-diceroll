# Ticket #4 — seeded expedition session

Sole implementation writer/editor owner on `feature/expedition-v1`; fixed baseline `c1be5672308d0d604bb08c2a1f44c688ffa2fc5f`. No branch/checkout, push, PR, tracker mutations or external messages. Root owns external delivery and ticket #5. Skills: implement, TDD, codebase-design, code-review; native validation uses cua-driver.

Read requested project/domain/architecture/spec/ticket/rules and party session with both review axes; fetched full issue #4/comments via explicit repository. Doctor passed on host Python 3.15 and pinned UE 5.8.4 CL58902052. Initial status at 2026-10-31 03:11:11 UTC: no project processes or MCP listener. Source/editor ownership accepted from root.

## Decisions and seams

Approved seam: public `FExpeditionRun` commands and value snapshot. Run owns the sole battle plus reflected progress. Read-only combat queries serve presentation. Snapshot assembles both without duplicating writable battle state. Offers are generated once, retain encounter seeds and exact preview facts, and history retains selected opportunity/outcome/recipient/survivor count. Generation uses its own retained random stream. No disk adapter or writable restore interface in this slice.

Three combats. Explicit loot after the first two victories; selected conscious survivor receives +1 gear (cap 3) and route-specific potions. One take/skip recovery stage after first reward; taking it grants conscious survivors +10 HP (capped) and +1 role use/slot (capped at 2). No potions, revival, or silent rest. A dangerous route secures supplies, adding one potion to the next guarded opportunity; its preview discloses the effect. Third victory recovers artifact and ends run; wipe ends immediately. Fresh NewRun(seed) resets party power and history.

## Validation in progress

Compiled fresh-start tracer failed before implementation: `.local/automation/20261031T031212025640Z` (three expected assertions). Build evidence: `.local/run-red-build.log`.

Fresh-start green: `.local/automation/20261031T031255919962Z` (1/1). Route red: `.local/automation/20261031T031333521283Z`; route green: `.local/automation/20261031T031441381476Z` (2/2). Full-flow stub red: see `.local/run-flow-red-test.log`; green `.local/automation/20261031T031721887737Z` (3/3). Initial expanded full Project run: `.local/automation/20261031T031825115622Z`, 25/25 with zero warnings. Offline helpers: 22/22 in `.local/run-helper-tests.log`. Added skip-rest and victory-reset/terminal checks afterward. Engine-generated unrelated AndroidFileServer defaults removed from tracked config, as in previous slices.

Implementation composes `FPartyContinuity::Carry/Reward/Recover`; legacy Advance remains an atomic compatibility wrapper for party tests. Run exposes only const Battle/Progress and value Snapshot. Rejected commands validate phase/recipient before mutation; route start validates a candidate copy before replacing authority. No asset changes or helper rewrites. Runtime board tracks/destroys tiles/obstacles/units on each layout rebuild, and resets framing for grid width.

Independent two-axis review and native acceptance are pending at the initial candidate commit.
