# M2 contract: the revision 11 balance pass

Status: **child issue [#241](https://github.com/AustinKelsay/dnd-diceroll/issues/241) of spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239); covers [#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228) and [#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187) items 1, 3, 4 and 5.** Written 2026-11-15 under the spec's delegated authority, before any tuning.
- Fixed review baseline: #258's head `50f5dfc2` (M1, [#240](https://github.com/AustinKelsay/dnd-diceroll/issues/240): move-through as catalog revision 11), on which this branch is stacked.
- Measured baseline: revision 11 on that head, harness [07](../gameplay-depth/07-balance-baseline-contract.md) policy 3, the same 99 parties and 32 seeds (`20261109 + i`, route pattern `i`), under each of the four reward strategies of [contract 13](../gameplay-depth/13-progression-contract.md#6-harness-measurement-plan). The reference for "revision 10's level" is revision 10 on the same build.
- Everything changes **only Compact runs created under revision 11**, which is unreleased. Revisions 7–10 and SRD keep every rule and number for life.
- Exact numbers below are starting values, tuned with the harness and recorded as built in [M2-balance-report.md](M2-balance-report.md).

## 1. What the baseline says

Revision 11 on `50f5dfc2` against revision 10 on the same build (both reproduce [M1's section 6](M1-movement-contract.md#6-measurement) exactly):

| | Revision 10 | Revision 11 baseline |
| --- | --- | --- |
| Overall win rate, gear / reserve / honed / mixed | 78.6 / 78.2 / 75.0 / 77.8% | 84.4 / 85.7 / 83.0 / 85.4% |
| Deaths per run (mixed) | 0.98 | 0.63 |
| Rounds per fight (mixed); fights over 15 rounds; longest | 6.14; 172; 43 | 5.61; 140; 41 |
| Defeat share by fight 1–5 (mixed) | 0.1 / 0.1 / 1.3 / 4.7 / 17.1% | 0.0 / 0.0 / 0.8 / 2.5 / 11.7% |
| Specials per fight, fights 1–5 (mixed) | 6.25 / 4.49 / 1.70 / 1.37 / 1.24 | 6.23 / 4.54 / 1.71 / 1.38 / 1.26 |
| Revives per fight, fights 1–5 (mixed) | 0.34 / 0.45 / 1.32 / 2.31 / 6.70 | 0.20 / 0.35 / 1.16 / 2.00 / 6.54 |
| Slot 1–4 share of damage taken (rotation, mixed) | 31 / 24 / 22 / 23% | 33 / 25 / 21 / 21% |
| Slot 1–4 share of downs; of deaths | — | 41 / 24 / 19 / 16%; 28 / 24 / 24 / 24% |
| Monk / Paladin / class median (rotation win rate, mixed) | 70.0 / 83.4 / 77.8% | 80.9 / 88.0 / 84.8% (Fighter highest at 90.4%) |
| #228 P2 classes with a clear-best card; largest P3 gap | — | none (only ties and small differences); 2.7 points |

Reading it:
- **Easier, same shape.** Every strategy wins 6–8 points more and loses a third fewer heroes, but the shape holds: the finale is still where runs end, and contract 13's holds are all met.
- **The finale is a rescue loop.** A fight-5 encounter averages 6.5 revives. A hero rescued at 1 HP next to a foe is down again before it acts.
- **Slot 1 is the target.** Slot 1 takes a third of the damage and 41% of the downs; the other slots are near their fair share.
- **Specials fade** exactly as in revision 10: about 1.3 per fight in fights 4 and 5 against 6.2 in fight 1.
- **Cards are interchangeable.** No class has a clear-best card; Honed trails Reserve by small margins in most classes.
- **Monk** is the weakest class (−3.9 points from the median) and **Paladin** among the strongest (+3.2), with Fighter (+5.6) ahead of both.

## 2. Targets

Judged on the 32 base seeds (3,168 runs per strategy) with `tools/balance_report.py`'s statistics: paired differences over (party, seed), a seed-cluster bootstrap of 10,000 resamples from `random.Random(20261112)`, 95% percentile intervals, and contract 13's clear / small / tie verdicts. These measure one simple policy, not human difficulty or fun; Austin's playtest is the check for those.

| # | Target | Why | Measured as |
| --- | --- | --- | --- |
| D1 | **Revision 10's difficulty:** every strategy's overall win rate is 76–80%, and `mixed` (the default) is within 3 points of revision 10 `mixed` (paired) | move-through should make the game feel better, not easier | win rate of all 3,168 runs; paired difference against revision 10 |
| D2 | **The run keeps its shape:** under `mixed` the finale has the highest defeat share, and contract 13's other holds stand (rotation 55–90%, every representative party at least 35%, Dangerous at least 1.5x Guarded net HP lost in every fight) | the finale is still clearly the hardest fight | contract 13 section 6 holds |
| C1 | **#228 P2: a card is clearly best somewhere:** at least one class has a single card clear (≥ 5 points, interval excluding 0) against both others | #228 | contract 13's per-class paired differences |
| C2 | **#228 P3: no card far ahead:** the largest paired gap between the rotation win rates of `gear`, `reserve` and `honed` is at most 10 points | no trap card, no must-pick | contract 13 P3 |
| C3 | **#228 P4: bounded power:** `mixed`'s rise over revision 10 `mixed` is at most 8 points, with D2's holds | no power creep | contract 13 P4, against revision 10 |
| R1 | **#187 item 1: rescue holds:** quick re-downs (a rescued hero Downed again before its next own turn) per rescue at most half the baseline's, and no more fights over 15 rounds than revision 10 | the rescue-then-down loop is a time sink | `rescues.csv` (section 5); rounds per fight |
| S1 | **#187 item 3: specials reach the finale:** specials per fight in fight 5 under `mixed` at least 1.5x the baseline's | specials fade by fight 5 | `encounters.csv` `specials_used` by stage |
| T1 | **#187 item 4: no slot funnel:** under `mixed`, slot 1's share of the damage the rotation heroes take is at most 28%, and no slot's share of deaths exceeds 30% | slot 1 takes most hits and deaths | `heroes.csv` `damage_taken`, `survived` by slot |
| K1 | **#187 item 5: Monk and Paladin toward the pack:** under `mixed`, each is closer to the median class win rate than in the baseline, and within 4 points of it | Monk the weakest class, Paladin the strongest | rotation class win rates |
| G1 | Gates zero (policy errors, illegal proposals, passive ends, protection mismatches), zero stalls, same seed same results | contract 07 | harness gates |
| G2 | **Revisions 7–10 are unchanged:** revision 10 under all four strategies and revisions 7, 8 and 9 under `gear` give byte-identical `runs.csv`, `encounters.csv` and `heroes.csv` before and after, and seeded runs of every older revision hash to digests captured on `50f5dfc2` | a run keeps its rules for life | `cmp`; `Project.Balance.Revision10RunsArePinned`, `…Revisions7To9RunsArePinned` |

If C1 is inconclusive (some class has a card at least 5 points ahead of both others on the point estimates, but an interval includes 0), the report adds contract 13's one extra fixed-seed batch (`20261141 + i`) for every measurement of the kept step and decides on the 64 pooled seeds. A class whose point differences are all under 5 points cannot become clear and does not trigger it. Nothing else gets extra seeds.

*R1 as built:* the quick re-down baseline (27.2% of rescues) came from rerunning the revision 11 baseline with the `rescues.csv` instrumentation, whose three CSVs are byte-identical to the baseline's. Its long-fight reference is revision 10's 172 fights over 15 rounds rather than the revision 11 baseline's 140: D1 puts difficulty back at revision 10's, and tougher foes lengthen fights by design (amended before the kept step, report step 2).

## 3. Levers, smallest first, with bounds

Each lever is gated on `FExpeditionRun::PlaysRevision11Balance` (Compact, revision 11). Values live in `Revision11Rules` (`BuildCatalog.h`), each `static_assert`ed within its bound.

1. **Enemy target choice** (#187 item 4; `FCombatEncounter::bNearestTarget`). An enemy with no hero in reach walks toward the hero it can attack soonest: the fewest steps to a cell it can stop on and attack from, the lower HP and then cell order breaking ties. It attacks the nearest hero in reach (Manhattan), the lower HP and then roster order breaking ties. The older rule is "the first firing cell in cell order, then the first legal target in roster order". Deterministic, a pure function of the position, and it draws no random number. One rule, on or off; no tuning.
2. **Rescue** (#187 item 1; `bSturdyRescue`, `RescueShare`): restores max HP / `RescueShare` (at least 1) instead of 1 HP. Start 3 (a third); bound 2–4 (half to a quarter). A potion still heals 2d4+2.
3. **Cards refill** (#187 item 3, #228; `CardsRefill`): Reserve keeps its +1 cap on each pool and refills each of those pools to the new cap instead of adding one use. Honed keeps its rows and also refills the hero's pools to its caps. Both stay rewards the player chooses in place of Gear (once per hero), as their potions are; there is no other refill. *Honed's refill was added during tuning (report, step 6): with Reserve alone refilling, Honed fell 4.6 points behind Reserve and below D1's band.*
4. **Enemy numbers by route and stage kind** (D1, D2; `EnemyHpPercent`, `EnemyDamageDelta`): revision 10's foes with HP scaled, and a damage bonus added, per route (Guarded, Dangerous) and stage kind (approach, fights 1–2; pressure, 3–4; finale, 5). Bounds 90–130% HP and +0 to +2 damage in each of the six cells. Rosters, roles, previews, armour, attack bonus, dice, speed and ranges stay revision 10's, so the route cards stay truthful without new text. *First written by stage kind only; split by route during tuning (report, step 5), because a stage-only scale left fight 5's Dangerous ≥ 1.5x Guarded hold borderline.*
5. **Monk and Paladin profiles** (K1), only if K1 misses after levers 1–4: revision 11 HP and armour for those two classes' builds, applied when the run creates its heroes and checked on restore. Bounds ±4 HP and ±1 AC.

**Not levers here:**
- **Per-class Honed rows and a per-class Reserve size.** Both live behind `FBuildCatalog::Abilities(Hero)` and `Caps(Hero)`, which every reader calls with the hero alone and which revision 10 runs share. A revision 11 value there needs the run's revision in every reader (combat, facts, HUD, progression), which is outside this ticket's bounds. If C1 misses after the levers above, the report says so and leaves it as a design question.
- **A refill after each victory.** The game brief (#28: "no … refill … or automatic rest") and #112 ("no implicit … free refill") rule it out; Reserve's refill is a chosen card, not an implicit one.
- **Post-rescue protection.** It needs a new saved per-hero state (who is protected, until when), so a save schema change. Rescue HP (lever 2) answers the same problem without one.
- **The harness policy.** Policy 3 is unchanged, so the before and after differ only in the game. Its known limits (contract 07 section 3) are reported, not fixed here.

## 4. Boundaries

- **Revisions 7–10 and SRD:** unchanged for life (G2). The rules are derived from the saved ruleset and revision like M1's move-through (`ApplyRevisionRules` on the opening encounter, every layout and `Restore`), never saved.
- **Save schema 6:** unchanged. Restore already regenerates and compares every enemy stat against the run's revision, so the stage scaling needs no new field; Rescue's HP, the target choice and Reserve's refill leave only state restore already accepts (pools within `Caps`, HP within max).
- **Generation and RNG:** boards, rosters, encounter seeds and draws are revision 10's. Target choice draws nothing; the combat stream differs only because different attacks happen.
- **Presentation reads the rules:** Rescue's action text comes from `FCombatBattle::Describe` and its forecast from `Forecast`, which equal the committed result. Reserve's and Honed's card rules come from `FProgressionFacts::bCardsRefill`, and their before → after pools from the claim rule applied to a copy, as today. The party builder previews each hero with `FExpeditionRun::CreateHero`, the seam `NewRun` creates heroes with, so Monk and Paladin HP and AC match the run. No new screen, asset or input.

## 5. Measurement

- Each step is four full `Balance.Baseline.` sweeps (`gear`, `reserve`, `honed`, `mixed`) under one machine-lock hold at `--cores 2`, with `DND_BALANCE_REVISION=11`. The references are revision 10 under all four strategies and the revision 11 baseline, both on `50f5dfc2`.
- **Harness addition (no policy change):** each group also writes `rescues.csv` (party, seed, stage, route, outcome, rounds, revives, redowns, quick_redowns). A re-down is a down of a hero already revived in that fight; a quick re-down is one before the revived hero's next own turn. `runs.csv`, `encounters.csv` and `heroes.csv` keep their columns and bytes, so the revision 10 digests stay valid.
- The step analysis (`m2_analysis.py`, on the evidence branch) imports `tools/balance_report.py` for the bootstrap, verdicts and holds, and adds slot shares, class rates, specials by stage, the long-fight tail and the rescue counts.
- Every lever step reruns all four strategies. The report lists each step, kept or not.

## 6. Tests

- `Project.Expedition.Combat.Revision11EnemiesGoForTheNearestHero`: equally near, the lower HP; the nearest before the weaker; the walk goes to the weaker of two equally far heroes; the same position gives the same choice; the older rule keeps slot 1.
- `…Combat.Revision11RescueRestoresAThird`: amount, forecast, committed healing and text agree, at least 1 HP; the older rule restores 1.
- `…Run.Revision11CardsRefillPools`: Reserve and Honed after fight 1 refill under revision 11 (Reserve to its new cap, Honed to the caps); under revision 10 Reserve adds one use and Honed changes no pool. The query's after-values, the card rule and the Honed card's pool line match the claim, and the claimed run restores.
- `…Run.Revision11BalanceFollowsTheRevision`: the flags for every revision and SRD; each fight carries its run's rules; revision 11 foes are revision 10's scaled by route and stage with every other number equal; the real save round trip saves no flag and Restore derives it.
- `…Run.Revision11MonkAndPaladinProfiles`: revision 11 Monk and Paladin heroes carry the adjusted max HP and armour, revision 10 the catalog's; both checkpoints restore, and a revision 11 hero saved with revision 10's numbers is refused.
- `Project.Balance.Revision10RunsArePinned` (M1, unchanged) and `Project.Balance.Revisions7To9RunsArePinned` (new, digests from `50f5dfc2`).
- Existing tests whose revision 11 outcome changes keep their intent: either they pin revision 10 explicitly or take the new value, each recorded in the PR.
