# Compact gameplay depth

- Parent: [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112).
- Authority: Austin's 2026-11-02 request to deepen the discussed gaps systematically, end to end, full send; bounded design/tuning and slice decisions delegated to the program owner.
- Root owner/workspace: primary agent, `dnd-diceroll-depth`, branch `feature/gameplay-depth`.
- Starting baseline: staging `df25f32ae47dded0d1832cb9ef2eddaa251a52d9` (menu #110). Original checkout and other feature worktrees are preserved.
- Current slice: [#50](https://github.com/AustinKelsay/dnd-diceroll/issues/50), authoritative targeting information and simultaneous outcome previews. No gameplay implementation is claimed yet.

## Ordered delivery

1. Targeting clarity (#50), then actual action costs/availability (#58) and contextual onboarding.
2. Seeded full-run party baseline and evidence-led kit tuning.
3. Distinct encounter roles, layouts and finale pacing.
4. Compact progression/equipment choices, retaining two specials and levels 1–5.
5. Class/effect/audio consistency and material issue #105.
6. Integrated complete-run acceptance, documentation and release handoff.

The parent issue owns the approved program boundaries. Concrete child contracts and blocking references precede each implementation; later tuning is based on the measured baseline. The broader SRD, Chaos and hex-board proposals are not adopted by this program.

## Slice ledger

| Slice | Fixed review baseline | Implementation | Verification | Independent review | Remaining |
| --- | --- | --- | --- | --- | --- |
| #50 targeting | `df25f32` | Pending | Root doctor passes on Python 3.15 / UE 5.8.4; helpers 50/50; staged contribution and whitespace checks pass for this initial record; no source build yet | Pending | Implementation, tests, native acceptance, PR/integration |

## Coordination and evidence

The separate stability session owns #32/#35 and `feature/32-renderer-stability` in `t3-cf27a55f`. Coordination was sent before this program's implementation. Its debugger/native workload must not be interrupted or competed with. The original project's editor and shared MCP endpoint remain outside this task's ownership. No editor lifecycle or native app action was performed for this program.

The new root worktree's preflight refuses solely because the shared MCP port is occupied. Doctor passes with `/opt/homebrew/bin/python3`; the initial system `python3` attempts correctly refused Python 3.9.6. Record the interpreter explicitly in subsequent commands. Headless build/test/package scheduling still needs coordination with the host owner.

Current rendering/startup risks #32/#35, material warnings #105, pointer delivery, sound and human balance are not cleared by source automation. No merged improvement or new stable release is claimed in this initial record.

## Handoff

Root owns the full program and each slice's review/integration. Child handoffs must identify exact checkout, branch, baseline/head, source/asset fingerprint, checks and counts, and honest limitations. No child may use the original editor, mutate another checkout, merge, or close issues. Run records describe their own evidence; current-state documentation is reconciled at integration.
