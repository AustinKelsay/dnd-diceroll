# Slice 5 report: progression and replay choices, before and after

Child issue [#218](https://github.com/AustinKelsay/dnd-diceroll/issues/218), parent [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112). Contract: [13-progression-contract.md](13-progression-contract.md). Previous report: [12-encounter-identity-report.md](12-encounter-identity-report.md). Fixed review baseline: staging `6ee589d` (the merge of #214).

## How it was measured

- Harness [07](07-balance-baseline-contract.md), policy version 2, the same 99 parties and 32 seeds (`20261109 + i`, route pattern `i`): 3,168 complete Compact runs per measurement, five measurements per step. Each measurement was a full `Balance.Baseline.` under the machine lock at `--cores 2` (about 40 s of play per measurement).
- **Before:** this build, catalog revision 9 (`DND_BALANCE_REVISION=9 DND_BALANCE_REWARD=gear`).
- **After:** revision 10 under `gear`, `reserve`, `honed` and `mixed` (`DND_BALANCE_REWARD`).
- **Recipients and cards (policy 2):**
  - `gear` is policy 1: Gear to the survivor with the lowest gear rank, then the lowest id.
  - `reserve` and `honed` give that card when it is available, otherwise Gear. The recipient is the survivor with the fewest upgrades (gear rank + Reserve + Honed), then the lowest id.
  - `mixed` uses the same recipient. It takes Reserve when **each** of the recipient's special pools holds at most one use, read per pool and not summed, as the claim sees them: after the fight and before the camp. Otherwise it takes Honed when free, otherwise Gear. This reading was the lead's decision on #218; contract section 6 is clarified to match.
- **Statistics:** `tools/balance_report.py targets`. It uses paired differences over (party, seed), a seed-cluster bootstrap with 10,000 resamples from `random.Random(20261112)` and percentile intervals. Verdicts are clear when the interval excludes 0 and |d| ≥ 5 points, small when it excludes 0 and |d| < 5, and a tie otherwise. The holds use the rules of contract 13 section 6.
- **Dangerous / Guarded measure:** per fight, the mean net HP lost as a share of the party's maximum HP (report 12's measure, which reproduces its 1.8x–3.9x on #214's data). The contract's "1.6x–3.3x" quote does not match report 12, which this report follows.
- **What the numbers mean:** read every number as what this simple policy does, not as human difficulty. A class win rate is a **party-policy outcome**: the strategy applies to every claim the party makes. It is not the causal value of one card to that class.

## Comparisons

| | Result |
| --- | --- |
| **C1:** before-run against #214's retained revision 9 after-run (`evidence/pr214-d249faf8`, `balance/final2`) | **pass.** Every column of #214's header in all three CSVs is equal, as exact strings, with equal key sets. The appended columns hold only `G`, one per reward claimed (per run, `min(4, encounters_won)`), and `reserve = honed = 0`. `group.json` matches apart from the elapsed time, `policy_version` (1 → 2) and the new `reward_strategy`. |
| **C2 (P1):** revision 10 `gear` against the before-run | **pass.** Every column of all three CSVs is equal, the new columns included; `group.json` differs only in `catalog_revision` (9 → 10) and the elapsed time. |
| New fields under `reserve`, `honed` and `mixed` (`check-cards`) | **pass.** One card per claim; at most one `R` and one `H`; flags agree with the cards; end-of-run caps equal the build caps plus Reserve. |

The revision 9 path is byte-identical to #214's, and Gear under revision 10 is policy 1 exactly.

## Steps tried

| Step | Honed rows | Rotation win: before / gear / reserve / honed / mixed | Largest P3 gap | Mixed rise | Reserve specials, fights 4–5 | Stalls (before / reserve / mixed) | Kept |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | contract starting values (Strike/Push +2, Area +1, Surge +1, Missile +2, Healing +3, Cure +3, Wind +3, Precision +1, Protect +1, Swap +2, Rage +1) | 78.1 / 78.1 / 77.9 / 74.1 / 77.4% | 4.0 | −0.7 | 3.95x | 0 / 1 / 1 | no |
| **1** | **every row at its contract bound: Strike/Push +3, Area +2, Surge +2, Missile +3, Precision +2 (the others were already there)** | **78.1 / 78.1 / 77.9 / 74.3 / 77.5%** | **3.8** | **−0.6** | **3.95x** | **0 / 1 / 1** | **yes** |

- Reserve's +1 is fixed by the contract (section 3), so the only lever is the Honed amounts, applied smallest first. Step 1 used the whole bound; there is no step beyond it (the lead's instruction: do not go beyond the contract bounds).
- Step 1 changes only `honed` and `mixed`: its before, gear and reserve CSVs are byte-identical to step 0's.
- The kept step is the PR head's rules code; after it only comments and docs changed.

## Targets

| # | Target | Result (step 1, 32 seeds) | Verdict |
| --- | --- | --- | --- |
| P1 | Gear is revision 9 (C2) | C2 passes | **met** |
| P2 | Each of `gear`, `reserve`, `honed` is best (clear against both others) for at least two of the 13 classes | No class has a clear best. Gear and Reserve tie in 10 classes. Gear is small-ahead of Reserve for the sorcerer and small-behind for the druid. Honed trails Gear in 12 classes (+2.0 to +4.5 points, small) and trails Reserve clearly for the druid and ranger. | **missed, accepted by the lead 2026-11-13, follow-up [#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228).** Honed is never even possibly best, so the extra batch cannot change it. |
| P3 | Largest paired gap between the rotation win rates of the three single-card strategies at most 10 points | gear − honed +3.8 (+2.0 to +5.8); honed − reserve −3.6 (−5.5 to −1.8); gear − reserve +0.2 (−1.4 to +1.8) | **met** |
| P4 | `mixed` rises at most 8 points over the before-run, and #188's slice 3/4 targets still hold | See the P4 breakdown below. | **met** |
| P5 | Under `reserve`, specials per fight in fights 4–5 at least 1.5x the before-run's | 3.95x (3.61 to 4.38) | **met** |
| P6 | Gates zero, same seed same results, no more stalls than the before-run | Gates are 0 in every measurement, and `Project.Balance.SameSeedSameResults` passes. Stalls: 0 before, 0 gear, **1 reserve**, 0 honed, **1 mixed**: one #217 deadlock (below). | **missed.** The known #217 limit, **accepted for the release by the maintainer on 2026-11-13.** |

P4 breakdown:
- **Rise:** −0.6 points (−2.7 to +1.3).
- **Rotation:** 77.5% (75.9–79.0%), inside 55–90%.
- **Representative parties:** every one at least 35%; the lowest lower bound is support-poor at 51%.
- **Dangerous at least 1.5x Guarded net HP lost, fight by fight:**

  | Fight | Ratio | Interval |
  | --- | --- | --- |
  | 1 | 2.31x | 1.97–2.75 |
  | 2 | 2.38x | 1.93–2.98 |
  | 3 | 2.37x | 2.00–2.81 |
  | 4 | 3.97x | 3.11–5.43 |
  | 5 | 1.85x | 1.55–2.20 |

  No resample was decided by the sign rule.
- **Finale highest defeat share:** the finale's share minus each other fight's is +17.0, +17.0, +15.8 and +12.4 points (every lower bound above +8.3).

**Non-dominance is not established.** Honed is clearly beaten for the druid and ranger, but Gear and Reserve are not clearly beaten anywhere.

**Extra batch:** none of P2–P5 is inconclusive and no P4 hold is borderline, so the extra fixed-seed batch (`20261141 + i`) was not needed. `DND_BALANCE_SEED_BASE` supports it.

## What the cards do here

| Specials per fight | Fight 1 | Fight 2 | Fight 3 | Fight 4 | Fight 5 |
| --- | --- | --- | --- | --- | --- |
| before / gear | 6.25 | 3.80 | 0.86 | 0.46 | 0.26 |
| reserve | 6.25 | 4.52 | 1.83 | 1.50 | 1.39 |
| honed | 6.25 | 3.87 | 0.86 | 0.44 | 0.27 |
| mixed | 6.25 | 4.49 | 1.70 | 1.37 | 1.24 |

- **Reserve answers #187 item 3 for the player who takes it.** Specials in fights 4–5 nearly quadruple, at no cost in wins. Its extra uses replace the +1 gear damage almost exactly.
- **Honed is the weak card under this policy.** The policy spends both specials' pools by fight 2, so a honed special is rarely cast again, while Gear's +1 applies to every basic attack. Raising every row to its bound moved Honed's rotation rate only from 74.1% to 74.3%.
- **Mixed is close to Reserve:** 77.5% against 77.9%, and 1.24–1.37 specials in fights 4–5.

## P6: the stall (#217)

The same run stalls under `reserve` and `mixed`, as a deadlock: rot5-04, seed 20261116, route pattern 7, fight 3 on the cairn field.
- Hero 4 (barbarian.wild-magic) stands at (7,7), sealed in by the bodies of the skirmisher at (7,6), the raider at (6,7) and the archer at (8,7), and by the cairn at (7,8).
- The brute at (4,3) can never reach it, and it can never reach the brute.

Revision 10's generation is revision 9's exactly. The fight differs only because earlier reward claims changed how the party fought.

This is [#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217)'s known risk: three bodies beside one obstacle, which no layout can rule out.
- The maintainer accepted it as a known limit for this release on 2026-11-13. [#226](https://github.com/AustinKelsay/dnd-diceroll/pull/226) (in review) gives a player who meets it an in-game way out with Abandon run.
- The sweep's fail-on-deadlock rule is unchanged, so `Balance.Baseline.Rotation5` fails under `reserve` and `mixed`. `Balance.Baseline.*` is outside the `Project.` gate.
- The rule fix follows the release with its own contract.

## Follow-ups

1. **P2 missed: make the cards real choices** ([#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228); the miss was accepted by the lead for the #112 release on 2026-11-13). Under this policy Gear and Reserve are near-equivalent, and Honed is behind everywhere, even at its bounds. A Honed that matters needs specials that are still being cast in the late fights: Reserve plus Honed together, a policy that saves uses, or larger Honed bounds. Any of those is a new contract decision. The class table above shows where each card stands.
2. **#217:** bodies can seal a hero in; see P6.
3. **Presentation (PR B):** reward cards, the `+1 Lv` markers, hero details, and the dock and party cards using per-hero caps and honed amounts. Until then the game's only claim path is Gear.

## As built (PR A)

- **Cards and command:** `ERewardCard { Gear, Reserve, Honed }`, `FExpeditionRun::ClaimReward(Id, Card, Error)` and the session's committed overload. The Gear overload remains.
  - Named rejections: `Loot can be claimed once…`, `Choose Gear, Reserve or Honed.`, `Select a conscious survivor…`, `Gear only in this run.`, `Reserve taken.`, `Already honed.`
  - Each rejection leaves the full snapshot and both random streams unchanged; a failed checkpoint write leaves the file bytes unchanged.
- **Seams:**
  - `FBuildCatalog::Abilities(Hero)` holds the honed amounts and ranges and a description regenerated from the facts.
  - `FBuildCatalog::Caps(Hero)` gives the live caps, adding +1 on every pool the build has; `Catalog.ReserveCapsEverywhere` shows these are exactly the pools its specials spend, for all 70 builds.
  - `FBuildCatalog::HonedBonus(Hero, Command)` covers Second Wind, Cure Wounds, Magic Missile, Precision Strike and Rage.
  - `FBuildCatalog::GuardAmount(Guard)` gives the Kit-paid stored amount, or Shelter's Rage-paid fact.
  - The values are in `UpgradeRules` (`BuildCatalog.h`), each `static_assert`ed within its contract bound.
- **Inventories, rerun on `6ee589d` and at the head:**
  - **Every honed hit has moved to the seams.** These remain:
    - the `BuildRules` constants;
    - the seams' own reads;
    - the Rage stance tests (`ActionFacts.cpp:235`, `CombatBattle.cpp:122`, `:135`, `:946`, `:962`);
    - the SRD Magic Missile path (`:801`–`:807`), where SRD heroes are never honed;
    - the Magic Missile feedback (`:825`), which names only the dart count, which never hones;
    - the presentation readers that PR B moves (`ExpeditionHUD.cpp:752`, `:811`; `ExpeditionPlayback.cpp:69`).
  - **Every cap hit moved to `Caps(Hero)`.** These remain:
    - the catalog definitions;
    - the kit-style tests (`B->KitCap > 0`, `Status`, `IsRageStyle`, validation `:115`, `:123`);
    - the fixture-only `ResourceCap` path for build-less heroes;
    - the HUD party-card groups (`ExpeditionHUD.cpp:745`–`:749`, PR B).

    The harness now exports end-of-run `Caps(Hero)`.
  - **New hit since `5bc0746`:** `ProgressionQuery.cpp`, which reads the same seams.
- **Shelter:**
  - Text: like every guard, Shelter ends when its Barbarian's next own turn starts (`AdvanceTurn`), and sooner when that Rage ends. Contract 13's "while its Barbarian rages" was wrong about the unchanged rule. The lead approved the corrected row:
    - status: `until next own turn or Rage ends`;
    - ability text: `until its next turn or its fury ends`. The #58 cost-word lint (`DescriptionsHaveNoCostWords`) keeps resource names such as "Rage" out of descriptions, so the ability text says "fury".
  - Tests: `Catalog.HonedShelterProtectsThroughRage` covers the honed 4 and the unhoned 3, a real hit, a checkpoint through the real save, the end with Rage, and the end at the next own turn while still raging.
- **Previews:** revision 10 Compact routes 1–4 read `1 upgrade choice, +N potion(s)`. Revisions 7–9, SRD and the finale keep their wording, and restore validates each run's own.
- **Camp:** `Survivors: +10 HP, +1 Wind/slot/Rage/Kit up to each hero's own cap. No revival or potions.`
- **`FProgressionQuery`:**
  - Per hero: level, level before, levelled, the basic-damage parts, gear rank, Reserve, Honed, caps and pools.
  - In the Reward phase, the three cards with availability, a note (`Fallen`, `GearOnlyRun`, `ReserveTaken`, `AlreadyHoned`, `GearAtCap`) and their before → after values.
  - In the Recovery phase, the camp's before → after values.
  - After-values come from the reward and camp rules applied to a copy of the battle; nothing rolls.
- **Revision 10 and schema 6:**
  - Restore accepts revisions 7–10.
  - `FExpeditionSave::Version` is 6. The decoder steps 4 → 5 → 6 explicitly. The 5 → 6 step refuses a schema 5 document that already has schema 6 fields.
  - A missing, fractional or non-numeric version is refused; version 7 is refused and preserved by New Run.
  - Real schema 5 fixtures from unmodified `6ee589d` (revisions 8 and 9; route, reward and mid-battle, with staging's own continuations) are in `Source/DnDDiceRoll/Tests/Fixtures/Schema5/`.
- **Harness:**
  - Policy 2, with `DND_BALANCE_REWARD` (default `mixed`) and `DND_BALANCE_SEED_BASE`.
  - `heroes.csv` appends `reward_cards`, `reserve` and `honed`; `group.json` gains `reward_strategy`.
  - `balance_report.py` gains specials per fight by stage, and the `compare`, `check-cards` and `targets` commands, with helper tests.
- **Fixture changes in existing tests:**
  - `CompactRevision9SeedFingerprint` and its twin create revision 9 explicitly; its golden is unchanged.
  - The "unsupported revision" cases moved from 10 to 11.
  - `Schema4UpgradeRoundTripsCurrentRuns` covers revisions 8, 9 and 10 and also strips the schema 6 fields.
  - The catalog-revision pin moved to 10.
  - No full-run outcome changed, so no `FirstSeedWhere` change was needed.
