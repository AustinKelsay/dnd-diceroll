# Compact gameplay depth

- Parent: [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112).
- Authority: Austin's 2026-11-02 request to deepen the discussed gaps systematically, end to end, full send; bounded design/tuning and slice decisions delegated to the program owner.
- Root owner/workspace: primary agent on PlebDev Ultra, T3-bound `feature/50-targeting-ultra`. Historical Mac source `dnd-diceroll-depth` / `feature/gameplay-depth` is preserved at the migration handoff.
- Starting baseline: staging `df25f32ae47dded0d1832cb9ef2eddaa251a52d9` (menu #110). Original checkout and other feature worktrees are preserved.
- Current slice: [#50](https://github.com/AustinKelsay/dnd-diceroll/issues/50), targeting information and simultaneous outcome previews, delivered by [PR #137](https://github.com/AustinKelsay/dnd-diceroll/pull/137). Implementation and Ultra automation are complete; on-screen acceptance of the final head is still open (see the table). Mac work is quiesced. The [migration handoff](03-desktop-handoff.md), [desktop corrections](04-ultra-review.md) and [review ledger](02-targeting-review.md) are historical; [overlay and review follow-ups](05-overlay-targeting.md) hold the latest.

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
| #50 targeting | `df25f32` | Mac source `661e216`; desktop placement/semantics `6988b7e`, connector clipping `86c86c4`, fixed-text frames `4f3035f`; overlay gate `210abed`; review follow-ups (one `ShowsTargeting()` gate, name-led chips with the qualifier preserved, Rage text, NDEBUG-safe native test) in [05](05-overlay-targeting.md) | Ultra (Linux, UE 5.8.4): fresh-checkout gate, full `Project.` 118/118 with zero warnings, helpers 60/60, native layout test passes with and without `-DNDEBUG`, packaged headless probe clean. Earlier Mac figures (84/85) are superseded. | Rounds 1-4 of source review cleared at `4f3035f`; PR #137 Standards and Spec reviews found no correctness bugs and requested the follow-ups above | On-screen recheck of the final head: Return-to-menu confirm with nothing drawn over it, dense and long-name labels, a multi-victim area frame, at 1280x720 and a larger size, keyboard and pointer. Awaits a confirmed free, awake display. |
| #179 balance baseline (slice 3) | `7f29981` | Seeded complete-run harness in test code (`Tests/Balance*`), report tool `tools/balance_report.py`; [contract](07-balance-baseline-contract.md), [baseline report](08-balance-baseline-report.md). No tuning. | Ultra (Linux, UE 5.8.4), `--cores 2`; merged in PR #180 (`bd02ede`) | Two rounds, PASS WITH FIXES, all addressed | Delivered |
| #184 balance tuning (slice 3 part 2) | `bd02ede` | Compact enemy stats (catalog revision 8, SRD unchanged); [contract](09-balance-tuning-contract.md), [before/after report](10-balance-tuning-report.md) | Ultra (Linux, UE 5.8.4), `--cores 2`: see the PR | Arranged by the coordinator | On-screen play check; special-economy, enemy-targeting and Monk follow-ups |
| #188 encounter identity (slice 4) | `14e1384` (merged with `cb25a41`) | Stages, layouts 3–6, saved roles, risk previews, catalog revision 9; [contract](11-encounter-identity-contract.md), [before/after report](12-encounter-identity-report.md) | Ultra (Linux, UE 5.8.4), `--cores 2`: see the PR | Arranged by the coordinator | On-screen play check (PR A list); PR B foe looks from the saved Role |

## Coordination and evidence

Ultra is primary and the validated host. The setup owner allocates native workloads on it one at a time (`--cores 2`); see "Shared machine" in [development](../../development.md). Current evidence and the pending on-screen acceptance are in [05-overlay-targeting.md](05-overlay-targeting.md). The [desktop record](04-ultra-review.md) is the earlier review history.

### Historical Mac coordination

The separate stability session owns #32/#35 and `feature/32-renderer-stability` in `t3-cf27a55f`. Coordination was sent before this program's implementation. Its debugger/native workload must not be interrupted or competed with. The original project's editor and shared MCP endpoint remain outside this task's ownership. No editor lifecycle or native app action was performed for this program.

The new root worktree's preflight refuses solely because the shared MCP port is occupied. Doctor passes with `/opt/homebrew/bin/python3`; the initial system `python3` attempts correctly refused Python 3.9.6. Record the interpreter explicitly in subsequent commands. Headless build/test/package scheduling still needs coordination with the host owner.

Current rendering/startup risks #32/#35, material warnings #105, pointer delivery, sound and human balance are not cleared by source automation. No merged improvement or new stable release is claimed in this initial record.

## Targeting source handoff

The child delivered `4d645b1` from isolated `t3-3aadf8f2`; its local merge baseline `4ad2f3c` has exactly the same tree as staging `df25f32`. Only the scoped implementation commit was cherry-picked into this root branch. The child's [source record](01-targeting.md) retains its original evidence and limitations.

Root static inspection found that the new test's fabricated spent Bonus Action conflicts with strict restore validation for heroes whose corresponding resource is untouched. The fixture now spends only the Action budget in that restore case. Each authored ability starts with an independent battle, and Barbarian signatures reach the following own turn through real EndTurn commands so Frenzy is actually exercised. A second adjacent enemy exercises nontrivial area victim ordering. These are source-level corrections, with no executable red/green claim yet.

The stability owner released bounded build/test slots on 2026-11-02; package/native QA was never released or run. The final active build finished successfully after the owner directed migration to desktop. No further Mac tests or engine jobs will run. Fresh root status before the jobs reported no editor associated with this checkout and rejected the other project's listener PID 50002. That editor remains untouched. Independent review was not started; a preparatory snapshot file is not review evidence.

## Handoff

Root owns the full program and each slice's review/integration. Child handoffs must identify exact checkout, branch, baseline/head, source/asset fingerprint, checks and counts, and honest limitations. No child may use the original editor, mutate another checkout, merge, or close issues. Run records describe their own evidence; current-state documentation is reconciled at integration.
