# Slice 3 part 2 contract: Compact balance tuning

Status: **child issue [#184](https://github.com/AustinKelsay/dnd-diceroll/issues/184); written 2026-11-10 under [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112)'s delegated authority.** Fixed review baseline: staging `bd02ede` (#180 merged). The baseline measurement is [08-balance-baseline-report.md](08-balance-baseline-report.md); the harness, policy, seeds and metrics are [07-balance-baseline-contract.md](07-balance-baseline-contract.md) and are **not changed** here (policy version 1, the same 3,168 runs). The before/after evidence is [10-balance-tuning-report.md](10-balance-tuning-report.md).

## 1. What the baseline says

Every one of 3,168 runs won all five fights; a fight costs the party 2–9% of its HP; the closest call in 15,840 fights left 41% of party HP; 0.006 deaths per run. Dangerous fights cost more than Guarded ones but neither threatens the run, and the finale is not the hardest fight. Melee classes deal a third less damage than the others, and all-melee parties are the slowest. Specials are gone after fight 2 under the greedy policy.

## 2. Targets (design proposals, not universal numbers)

Each target is drawn from the [game brief](../../game-brief.md) and the baseline. None is a claim about human difficulty: the harness policy is simple, so a human who retreats, saves resources and positions should do better than these figures. Each must be supported or refuted by the before/after report; a target the evidence does not reach is reported as missed, not redefined.

| # | Target (proposal) | Why (brief / baseline) | Measured as |
| --- | --- | --- | --- |
| T1 | **The run can be lost, and a sensible party usually wins**: rotation win rate in 55–90%; every representative party at least 35% | Brief: "finish or lose the expedition", "a party wipe ends the expedition"; baseline 100% | win rate with 95% Wilson interval |
| T2 | **Losses without ending the run**: at least 15% of victories lose at least one hero | Brief: "dead adventurers remain lost; survivors continue", "a smaller surviving party can continue"; baseline under 1% | share of won runs with deaths |
| T3 | **Attrition across the run**: in fights 4 and 5 the mean party low point is below 60% of party HP, and at least a quarter of fight-5 fights go below 50% | Brief: health and resources persist, one recovery; baseline 73–82% and worst 41% | lowest party HP per fight |
| T4 | **The route choice matters**: a Dangerous fight costs at least 1.5 times the HP of a Guarded fight at the same stage, and loses more often | Brief: route choices "with partial risk/reward information"; baseline 1.7–3.7x HP but no losses | HP lost and defeats by stage and route |
| T5 | **The finale is the climax**: fight 5 has the highest defeat share of the five stages (both routes together) | Brief: "five combat encounters including that finale" | defeats by stage |
| T6 | **Melee is not dominated**: the all-melee and Fighter-duplicate parties within 20 points of the ranged party's win rate; the melee damage-share gap no wider than the baseline's 33% | #112 "subclass usefulness"; baseline melee 7.0 vs 10.5 damage per fight | party win rates, damage share |
| T7 | **Specials matter through fight 5**: reported, not targeted here (section 4) | baseline 0.25 specials per fight in fight 5 | specials per fight by stage |
| T8 | Gates unchanged: 0 policy errors, illegal proposals, passive ends, protection mismatches and stalls; same seed, same results | contract 07 | harness gates |

## 3. Levers, smallest first

Every lever is **Compact only**: SRD mode (`ERuleset::SRD`) keeps every number, and an SRD run of a given seed plays exactly as before. Levers are added one at a time; a lever is kept only when the before/after evidence moves its target without breaking another, and the report lists each tried step, kept or not.

1. **Enemy pressure** (`FExpeditionRun::Generate`, Compact branch): the stats of the existing roster (raiders and the archer): HP and its growth per stage, armour, attack bonus, damage die and bonus, by route (Guarded, Dangerous) and stage. Enemy **count and roles stay as today** (Guarded 2 with an archer, Dangerous 3 with an archer), so the route previews stay truthful without new text, the 16–18 by 11–12 grids, spawn columns and the 5–7 cell opening gap are unchanged, and slice 4 keeps the composition decisions. Bounds: enemy HP at most double the current value at each stage, attack bonus at most +5, damage die at most d8.
2. **Melee walking cost**, only if T6 still fails after lever 1: Compact Fighter, Barbarian, Monk and Paladin heroes move 5 cells instead of 4, applied at party construction and checked on restore (the same place SRD adjusts initiative). The opening gap stays.

Considered and **not changed in this PR**, with the reason:

- **Kit and Rage budgets across five fights** (caps, camp allowance): the caps live in the build catalog shared with SRD mode, the HUD reads `FBuildDefinition::KitCap`/`RageCap` directly (a `Presentation/` change, which this program leaves to the graphics overhaul), and the camp text `FExpeditionRun::RecoveryDescription` is shared by both rulesets. A per-fight refill would contradict the brief's single recovery and #112's "no implicit … free refill". Follow-up: a small contract for a Compact view of resource caps (rules facts plus the HUD reading them), agreed with #95.
- **Barbarian Rage gating** (duration or gating): the Rage duration is described in the catalog's shared ability text ("through next own turn"), so a Compact-only change would make the SRD text or the Compact text wrong. Same follow-up.
- **Melee-range specials on ranged heroes** (War Strike, Thunder Shove, Spore Cloud, Singing Blade, Ambush): their ranges are catalog ability facts shared with SRD mode. Same follow-up.
- **Enemy target choice** (slot 1 takes most hits): enemy behaviour; its own contract.

## 4. Boundaries

Not changed: the save schema (5); the 70 kits' identities, abilities, costs and texts; two specials per hero; five encounters; levels 1–5; the artifact-only fifth victory; the route structure, rewards, camp and its text; enemy behaviour (`PlayEnemyTurn`); every SRD-mode number; `Presentation/`, capture, camera and world code; `Config/` and `Content/`.

**Versioning** (revised after review round 1: existing saves must keep resuming, per the [#95 decision](https://github.com/AustinKelsay/dnd-diceroll/issues/95#issuecomment-6609461075) that schema 4 saves load as Compact):
- Compact enemy stats are part of the rules interpretation a checkpoint is restored against (restore regenerates the encounter and compares every enemy stat), so new runs are created under `FBuildCatalog::Revision` 8.
- Restore supports revisions 7 and 8 explicitly. A run keeps its revision for its whole lifetime: a revision 7 run (including format 4 saves) keeps revision 7 enemy generation and validation for already-spawned and future encounters alike, through a revision-aware stats function.
- Unknown revisions are refused by name and preserved on disk as before.
- Schema 5 is unchanged.

**Tests whose expectations change**, each updated deliberately with the reason in the test:
- `Project.Expedition.Run.CompactSeedIdenticalFingerprint`: the seed-42 Compact golden fingerprint is recaptured under revision 8. The test still proves seed identity (twin runs) and Victory encoding.
- `Project.Expedition.Persistence.Schema4LoadsAsCompact` stays exactly as on staging: both revision 7 fixtures load as Compact and resume identically. A new round trip, `Schema4UpgradeRoundTripsCurrentRuns`, writes a revision 8 run as format 4 and proves it decodes, restores and continues identically. A new `SrdRevision7Resumes` covers a revision 7 SRD checkpoint.
- Any test that asserts the catalog revision, or that the old enemy stats or full-run outcomes hold, keeps its intent with the new value.

**Slice 4 (encounter identity)** comes next and will re-tune enemies per encounter (roles, compositions, finale). This PR sets a measured pressure level and leaves the harness for slice 4 to rerun; slice 4 may replace these stats as long as it reports the same before/after tables.

## 5. Evidence

Before: the committed baseline (`bd02ede`, policy 1). After: the same 99 parties, 32 seeds and policy at the tuned head. Report tables: overall and per party, per class and per kit, per stage and route, deaths and their causes, and every tried lever step. Checks: build with 0 warnings, full `Project.` with 0 warnings, helper tests, `contribute.py check`, `Balance.Baseline.` before and after, and data on an evidence branch. Because gameplay numbers change, the PR lists a short on-screen play check for a human-style run.

## 6. As built

- **Lever 1 kept**, step 3 of four measured steps (the [report](10-balance-tuning-report.md) lists each). Compact enemies at fight s = 0–4:
  - HP 20 + 4s (Guarded) or 24 + 4s (Dangerous);
  - armour 12 or 13;
  - attack +4, +4, +5, +5, +5;
  - damage d6 (Guarded) or d8 (Dangerous), plus 3, 3, 3, 4, 4.

  All within the bounds of section 3. `EnemyStats` in `Run/ExpeditionRun.cpp` holds both rulesets' tables; SRD keeps revision 7.
- **Lever 2 not applied:** T6 passes after lever 1 (melee party 90.6%, Fighter duplicates 100%, ranged 87.5%; melee damage gap 18%).
- **Targets:** T1–T6 and T8 met (T1 and T2 narrowly), T7 reported as deferred. Rotation win rate 86.7% (85.5–87.9%), 16.1% of victories lose a hero, finale defeat share 7.7%.
- **Versioning:** new runs use `FBuildCatalog::Revision` 8, and restore accepts `MinimumRevision` 7 to 8. `EnemyStats(Ruleset, Revision, …)` has explicit Compact and SRD cases and fails for unsupported values. Revision 7 Compact runs keep revision 7 enemies for life. `FExpeditionRun::Restore` refuses other revisions by name, and `NewRunPreservesUnreadableCheckpoint` proves an unsupported future revision is preserved on New Run.
- **Tests:**
  - new: `Run.CompactEnemyTuning` (stats per fight and route, counts and roles, named refusal) and `Run.SrdSeedUnchangedByCompactTuning` (SRD golden captured on revision 7 code);
  - recaptured: the Compact fingerprint golden (seed 42 is a new, revision 8 run);
  - `Schema4LoadsAsCompact` unchanged from staging; new `Schema4UpgradeRoundTripsCurrentRuns` and `SrdRevision7Resumes`; `CompactEnemyTuning` also pins revision 7 enemies for a restored revision 7 run;
  - full-run tests whose subject is a completed run take the first winning seed from 42 on (`ExpeditionTestSupport::FirstSeedWhere`, asserted to exist).
