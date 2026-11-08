# Feature run: graphics overhaul

- **Issue/spec and approved acceptance:** parent [#138](https://github.com/AustinKelsay/dnd-diceroll/issues/138); child tickets #139–#152 (native sub-issues with blocked-by edges). Shared acceptance is in #138. The council plan is in [plan.md](plan.md).
- **Owner, branch, baseline, dependencies:** Austin (owner). Lead: Claude Opus 5.5; implementation: Claude Sonnet 5.5 subagents in isolated worktrees. One branch per ticket from `staging`; P0 `feature/139-capture-harness` and spike `spike/140-look-dev` from staging `d5a954e`. #141, #142, #146 and #151 also wait on #50 (PR #137).
- **Granted authority (2026-11-08):** Austin approved the council plan and its recommendations: a presentation-only lighter grid first, then bigger arenas in #151 after #112/#95 agree; a rigged-cast pilot, then all 13 classes; a dedicated Vault finale world; start P0/P1 now; fresh independent seats for Standards/Spec review. Ultra turn granted 2026-11-08 for builds, automation and short game runs (`--cores 2`, one job at a time; ask before packaging).
- **Decisions still with the owner:** gate 1 after #140 (camera, filtering, lighting, cell size, skeleton); gate 2 after #148 (vertical slice and performance); a spot check per #150 cast batch; #151 timing with #112 slice 4 and the #95 owner.
- **Current slice/status and next action:** #139 is implemented and in review; the #140 Blender half is done, and its Unreal A/B captures come next.
- **Editor owner and last observed state:** no editor running at the start (2026-11-08); P0 uses `-game` runs only, no editor/MCP.

## Slice ledger

| Issue/slice | Fixed review baseline | Commit | Checks and accessible evidence | Independent Standards/Spec review | Remaining work |
| --- | --- | --- | --- | --- | --- |
| #139 P0 capture harness | `d5a954e` | `10e16d8`, `c7d83a9` | Ultra, UE 5.8.4: build 0 warnings; `Project.Expedition.` 110/110, 0 warnings; `Project.Tools.CaptureFixtures` 1/1; helpers 69/69; `contribute.py check` PASS. Gallery: 4 fixtures × 2 resolutions, 94 shots per run; a repeat run had identical camera transforms and a mean pixel difference of 0.032/255 (max 0.239); primitives run 30 shots. Selected captures and a manifest: [`evidence/pr153` at `9873052`](https://github.com/AustinKelsay/dnd-diceroll/tree/98730529c3c38762b933cbcda02ca5488bfc3acc). | Round 1: Standards (GPT-6.1 Sol) PASS with fixes; Spec (GPT-6 Astra) FAIL on the missing enemy-turn shot. Fixed: capture runs bind no input; the `default-seed-enemy-turn` fixture; evidence linked; plan reconciled with approvals. After the fixes: build 0 warnings, `Project.Expedition.` 110/110 with 0 warnings, fixtures 1/1, helpers 69/69, gallery 10 runs / 120 shots. Round 2: pending. | review |
| #140 P1 look-dev spike | `d5a954e` | spike `933c221`..`9c8bd1c` (Blender half) | Metric Vigil world (27×18 m footprint, kept architecture with occluders split out, 1,209 unaligned paving stones), 25-bone rigged fighter (3,441 tris; idle and walk validated by reimport), Blender camera/grid/filter contact sheet. Unreal A/Bs pending. | n/a (spike) | Unreal captures, gate 1 |

## Coordination

- #112 (gameplay depth, Austin's other session): coordination comment posted 2026-11-08. #147/#150 deliver its slice-6 visual class identity; #149's Vault world serves its slice-4 vault finale; #151 waits for slice-4 agreement.
- #95 (SRD 5.2.1, Landon): no rules changes before #151.
- #32/#35 (renderer stability): their checkout and processes are untouched.

## Handoff

- Uncommitted/unfinished work and how it is preserved:
- Shared asset paths and any active reservations:
- PR, latest reviewed/tested head, CI/bot state:
- Parked blockers, known limits, and their owners:
- Integration result and issue closure (record after merge):
