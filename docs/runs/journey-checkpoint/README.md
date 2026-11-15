# Feature run: journey checkpoint

- Issue/spec and approved acceptance: spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239) (passable bodies, sound overhaul, war-table journey map, finale and polish), approved by Austin on 2026-11-15. Each child ticket carries its own acceptance; the checkpoint's integrated acceptance is in the spec.
- Owner, branch, baseline SHA, dependencies: the lead agent owns this run record. Children branch from `origin/staging` (`5223a0b4` or later). Blocking edges are listed in each ticket: #241 after #240, #243–#246 after #242, #248 after #247, #249 after #248.
- Granted authority and decisions still requiring the owner: agents do reviews, gates and `staging` merges under Austin's standing delegation (spec #239, "Owner and authority"). A save schema change, or a product decision a ticket does not settle, goes back to Austin.
- Current slice/status and next action: M1 ([#240](https://github.com/AustinKelsay/dnd-diceroll/issues/240), catalog revision 11 move-through) is in review; its contract is [M1-movement-contract.md](M1-movement-contract.md). M2 ([#241](https://github.com/AustinKelsay/dnd-diceroll/issues/241)) tunes balance inside the same unreleased revision 11 after it.
- Editor owner and last observed state (verify live before reuse): none recorded. Every Unreal job uses the Ultra machine lock; see [development](../../development.md#shared-machine).

## Slice ledger

| Issue/slice | Fixed review baseline | Commit | Checks and accessible evidence | Independent Standards/Spec review | Remaining work |
| --- | --- | --- | --- | --- | --- |
| #240 M1 move-through (revision 11) | staging `5223a0b4` | see its PR | see its PR and [M1-movement-contract.md](M1-movement-contract.md) | pending | the lead's review and the batched on-screen pass |

## Handoff

- Uncommitted/unfinished work and how it is preserved: none for M1 beyond its PR branch `feature/240-srd-move-through`.
- Shared asset paths and any active reservations: none for M1 (no binary assets).
- PR, latest reviewed/tested head, CI/bot state: recorded on each child PR.
- Parked blockers, known limits, and their owners: see each child ticket.
- Integration result and issue closure (record after merge):
