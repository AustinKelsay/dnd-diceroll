# Feature run: graphics overhaul

- **Issue/spec and approved acceptance:** parent [#138](https://github.com/AustinKelsay/dnd-diceroll/issues/138); child tickets #139–#152 (native sub-issues with blocked-by edges). Shared acceptance is in #138. The council plan is in [plan.md](plan.md).
- **Owner, branch, baseline, dependencies:** Austin (owner). Lead: Claude Opus 5.5; implementation: Claude Sonnet 5.5 subagents in isolated worktrees. One branch per ticket from `staging`; P0 `feature/139-capture-harness` and spike `spike/140-look-dev` from staging `d5a954e`. #141, #142, #146 and #151 also wait on #50 (PR #137).
- **Granted authority (2026-11-08):** Austin approved the council plan and its recommendations: a presentation-only lighter grid first, then bigger arenas in #151 after #112/#95 agree; a rigged-cast pilot, then all 13 classes; a dedicated Vault finale world; start P0/P1 now; fresh independent seats for Standards/Spec review. Ultra turn granted 2026-11-08 for builds, automation and short game runs (`--cores 2`, one job at a time; ask before packaging).
- **Granted authority (2026-11-09, coordinator on Austin's instruction):**
  - The run is fully agent-driven: nothing waits for Austin.
  - The lead merges each overhaul PR to `staging` once three gates pass: native evidence at the merging head, a fresh independent re-review of what changed (one seat covering both axes is enough unless the fix was large), and green CI.
  - Only `main` promotion stays with the coordinator.
  - Approved headless commandlet runs, each with a hash snapshot and diff and a stop on any unexpected change: #143's texture reimport and camera-clip master rebuild, #145's plain and replace runs, and #147's skeletal import.
  - The on-screen pointer and keyboard check for #142 and #146 does not block their merges; it runs once on the integrated staging build.
- **Decisions and gates:**
  - Gate 1 was decided by the owner on 2026-11-08 (see #140).
  - At gate 2 (#148) and at each #150 cast batch, the lead posts the captures and numbers, makes the call against the approved plan, and continues. Austin reviews afterwards.
  - #151 (bigger arenas) gets a concrete recommendation to the coordinator instead of parking.
  - Minor in-slice design calls are the lead's, recorded on the issue.
- **Current slice/status and next action (2026-11-09):**
  - **Merged to `staging`:** #139 (PR #153, `55902f0`), #141 (PR #154, `d0bf23b`), #142 (PR #160, `8e78270`), #146 (PR #155, `b02b5c3`) and the #157 helper fix (PR #161, `daf39d7`).
  - **In review after native verification:** #143 (PR #159), #145 (PR #163, stacked on #159) and #147 (PR #164, stacked on #159).
  - **Next:** merge as reviews pass, run the on-screen pass on integrated staging, then gate 2 (#148). #149's Root r002 Blender kit is being authored; #144 starts after #145 and #147 land.
- **Editor owner and last observed state:** no editor running at the start (2026-11-08); P0 uses `-game` runs only, no editor/MCP.

## Slice ledger

| Issue/slice | Fixed review baseline | Commit | Checks and accessible evidence | Independent Standards/Spec review | Remaining work |
| --- | --- | --- | --- | --- | --- |
| #139 P0 capture harness | `d5a954e` | `10e16d8`..`36ff3c0` | Ultra, UE 5.8.4: build 0 warnings; `Project.Expedition.` 110/110, 0 warnings; `Project.Tools.CaptureFixtures` 1/1; helpers 69/69; `contribute.py check` PASS. Gallery: 4 fixtures × 2 resolutions, 94 shots per run; a repeat run had identical camera transforms and a mean pixel difference of 0.032/255 (max 0.239); primitives run 30 shots. Selected captures and a manifest: [`evidence/pr153` at `9873052`](https://github.com/AustinKelsay/dnd-diceroll/tree/98730529c3c38762b933cbcda02ca5488bfc3acc). | Round 1: Standards (GPT-6.1 Sol) PASS with fixes; Spec (GPT-6 Astra) FAIL on the missing enemy-turn shot. Fixed: capture runs bind no input; the `default-seed-enemy-turn` fixture; evidence linked; plan reconciled with approvals. After the fixes: build 0 warnings, `Project.Expedition.` 110/110 with 0 warnings, fixtures 1/1, helpers 69/69, gallery 10 runs / 120 shots. Round 2: Spec (GPT-6 Astra) PASS with fixes (repeat the five-fixture gallery at a clean head); Standards (GPT-6.1 Sol) PASS with fixes (debug shortcuts, console and physical pointer still reached capture runs). Fixed in `0259562`. Clean-head evidence at `0259562`: build 0 warnings, `Project.Expedition.` 110/110 with 0 warnings, fixtures 1/1, helpers 69/69; two full galleries of 120 shots each, all camera transforms identical, mean pixel difference 0.023/255 (max 0.166); indexes on [`evidence/pr153` at `30d74e5`](https://github.com/AustinKelsay/dnd-diceroll/tree/30d74e590093aef1c86795fc52ff8cde003f858d). Round 3 (one fresh GPT-6.1 Sol seat, both axes): Spec PASS; Standards PASS with one fix (F11 and Alt+Enter fullscreen shortcuts still reached capture runs), fixed in `36ff3c0` (switched off in memory for capture runs only). At `36ff3c0`: build 0 warnings, `Project.Expedition.` 110/110 with 0 warnings, helpers 69/69, gallery 120 shots with all camera transforms identical to the `0259562` runs. | merged: PR #153 → `55902f0` |
| #140 P1 look-dev spike | `d5a954e` | spike `933c221`..`9c8bd1c` (Blender half) | Metric Vigil world (27×18 m footprint, kept architecture with occluders split out, 1,209 unaligned paving stones), 25-bone rigged fighter (3,441 tris; idle and walk validated by reimport), Blender camera/grid/filter contact sheet. Unreal A/Bs pending. | n/a (spike) | gate 1 decided by the owner on 2026-11-08 (#140 decision comment; Unreal A/B captures on `spike/140-look-dev` at `95cf93c`); spike branch is evidence only |
| #141 P2 board metric | `d9eac88` | PR #154 → `d0bf23b` | Build 0 warnings; `Project.Expedition.` 121/121, 0 warnings; every standing figure 175–185 cm (themed 177.5–183.4, primitive 177.6–183.3). | R1: Standards PASS; Spec PASS with fixes (height test, provenance). R2 (one Sol seat, both axes): PASS/PASS. | merged |
| #142 P3 contextual grid | `a269416` | PR #160 → `8e78270` | Build 0 warnings; 127/127, 0 warnings; every-catalog-action test (60 pairs, 9 area cases with victims); themed and primitive galleries. | R1: both PASS with fixes. R2: PASS/PASS. | merged; two-victim fixture follow-up; on-screen pass pending |
| #157 helper (failed lifecycle) | `d9eac88` | PR #161 → `daf39d7` | Helpers 71/71; the new tests fail on the old helper. | Sol, both axes: PASS/PASS. | merged |
| #146 P7 compact HUD | `d9eac88` | PR #155 → `b02b5c3` | Build 0 warnings; 130/130 with staging; coverage 13.99%; all-class subclass bounds, Menu, keyboard inspect. | R1: both FAIL (subclass overflow, keyboard details, Menu). R2: both PASS with fixes (docs). R3 (one Sol seat, both axes): PASS with fixes (description only, fixed). | merged; on-screen pass pending |
| #143 P4 camera and lighting | `a269416` → staging | PR #159 @ `5f87a96` | Build 0 warnings; 131/131; exact HUD-safe fit; anisotropic + linear mips (reimport: exactly 21 `T_*`); Masked camera clip (exactly the master changed); timings GPU +0.10 ms. | R1: Standards PASS with fixes; Spec FAIL (fit). R2: running. | merge on pass |
| #145 P6 Vigil r002 kit | #159 head | PR #163 @ `1033245` | Plain run: added exactly 65, changed DA. Replace run: exactly 4 meshes + their `T_*`/`MI_*` + DA. 138/138. | R1: running. | retarget after #159 |
| #147 P8 rigged pilot | #159 head | PR #164 @ `7cec21a` | Skeletal import: added exactly SKEL, 3 SK, 9 clips, 3 MI, 3 T; changed master flag + DA. 136/136; rigged figures 180.0 cm. | R1: running. | retarget after #159 |

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
