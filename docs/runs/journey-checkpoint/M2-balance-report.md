# M2 report: the revision 11 balance pass

Child issue [#241](https://github.com/AustinKelsay/dnd-diceroll/issues/241) of spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239); covers [#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228) and [#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187) items 1, 3, 4 and 5. Contract: [M2-balance-contract.md](M2-balance-contract.md). Baseline: M1's revision 11 ([#240](https://github.com/AustinKelsay/dnd-diceroll/issues/240), `50f5dfc2`, merged to staging as `91fd28b0`).

## How it was measured

- Harness [07](../gameplay-depth/07-balance-baseline-contract.md), policy 3 (unchanged), the same 99 parties and 32 seeds (`20261109 + i`, route pattern `i`): 3,168 complete Compact runs per strategy, under `gear`, `reserve`, `honed` and `mixed` ([contract 13](../gameplay-depth/13-progression-contract.md#6-harness-measurement-plan)). Every Unreal job ran on Ultra (UE 5.8.4) under the machine lock at `--cores 2`.
- **References,** both on `50f5dfc2`: revision 10 under the four strategies (`m2-r10-*`, "revision 10's level") and revision 11 (`m2-r11base-*`, the baseline). Both reproduce M1's section 6 exactly.
- **Statistics:** `tools/balance_report.py`'s paired differences over (party, seed), its seed-cluster bootstrap (10,000 resamples, `random.Random(20261112)`, 95% percentile intervals), contract 13's clear / small / tie verdicts and its holds. `m2_analysis.py` (evidence branch) imports it and adds slot shares, class rates, specials and revives by fight, the long-fight tail and `rescues.csv`.
- **Exploration:** during tuning, a temporary build read lever values from environment variables, so one build could sweep many values (`mixed` only). It was removed before the kept step. The final constants reproduce the explored step `hrpb` byte for byte (all four strategies' CSVs equal).
- Read every number as what one simple policy does, not as human difficulty or fun. Austin's playtest is the check for those.

## Steps tried

Each row is revision 11 under `mixed` (overall win rate of 3,168 runs), with the lever values named against the previous row. Quick re-downs are rescued heroes Downed again before their next own turn, as a share of rescues.

| Step | Change | Win | Deaths/run | Fight 5 defeat share | Fight 5 specials | Quick re-downs | Fights > 15 rounds | Kept |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | baseline (instrumented rerun; CSVs byte-identical to `m2-r11base`) | 85.4% | 0.63 | 11.7% | 1.26 | 27.2% | 140 | — |
| 1a | target choice only | 84.2% | 0.68 | 12.4% | 1.25 | 26.3% | 159 | yes |
| 1b | Rescue ⅓ only | 95.0% | 0.26 | 4.2% | 1.26 | 18.0% | 71 | yes |
| 1c | Rescue ¼ only | 90.8% | 0.42 | 7.7% | 1.26 | 23.5% | 101 | no |
| 1d | Reserve refill only | 86.7% | 0.58 | 10.4% | 3.17 | 26.9% | 113 | yes |
| 1 | 1a + 1b + 1d, foes unchanged | 95.9% | 0.22 | 3.2% | 3.14 | 16.8% | 68 | — |
| 2 | foes by stage: HP 110 / 120 / 130%, damage +1 / +1 / +2 | 78.0% | 0.93 | 17.9% | 3.05 | 22.2% | 380 | no: long fights |
| 3 | Rescue ½, foes at the lever bounds (130%, +2) | 86.7% | 0.59 | 7.8% | 3.00 | 11.9% | 200 | no: D1 out of reach |
| 3b | enemy attack tie-break: higher HP, or roster order, instead of lower HP | 78.1 / 77.2% | 0.91 / 0.96 | 16.1 / 16.9% | 3.06 | 21.7 / 22.5% | 228 / 221 | no: no effect on re-downs |
| 4 | foes HP 100 / 110 / 120%, damage +1 / +2 / +2; Monk +4 HP +1 AC, Paladin −2 HP | 79.2% | 0.88 | 15.2% | 3.09 | 22.4% | 257 | partly |
| 5 | foes split by route: Guarded HP 100% and +2 damage throughout; Dangerous HP 100 / 110 / 125%, +2 damage | 79.1% | 0.89 | 15.3% | 3.03 | 21.8% | 295 | yes |
| 5p | Paladin −4 HP (all strategies: gear 76.8, reserve 79.3, honed **74.7**, mixed 78.9%) | 78.9% | 0.90 | 15.5% | 3.03 | 21.9% | 296 | partly |
| 6 | Honed also refills; Paladin −4 HP −1 AC (gear **75.7**, reserve 77.7, honed 77.7, mixed 78.1%) | 78.1% | 0.94 | 16.1% | 3.13 | 22.4% | 296 | yes |
| **7** | **Guarded approach damage +1 instead of +2** | **78.4%** | **0.92** | **15.8%** | **3.16** | **22.4%** | **299** | **yes (head)** |

- **Rescue is the big lever.** A third of max HP alone adds 9.6 points; the target choice takes 1.2 away, and the Reserve refill adds 1.3. So most of the enemy scaling pays for Rescue.
- **Only Rescue HP moves re-downs.** Changing whom enemies attack first does not: a rescued hero stands beside its rescuer and the foes in reach of both. Rescue ½ halves the quick re-downs, but even with every foe at the lever bounds the party then still wins 87%.
- **Tougher foes mean longer fights.** Scaling HP pushes the long-fight tail up, nearly all of it in the Dangerous finale. Damage scales difficulty with less of that, so the kept step adds damage everywhere and HP only on the Dangerous road.
- **The Dangerous ≥ 1.5x Guarded hold went borderline at fight 5** once Rescue was stronger (lower bound 1.36–1.52). A stage-only scale could not fix it, so the enemy lever was split by route (contract section 3, as amended); from step 5 it is met.
- **Honed's refill (step 6).** With only Reserve refilling, Honed played 4.6 points behind Reserve and below D1's band, because its stronger specials were cast once and then never again. A Honed that also refills lands level with Reserve.

## Targets

Kept step, 32 seeds; revision 10 and baseline from `50f5dfc2`.

| # | Target | Result | Verdict |
| --- | --- | --- | --- |
| D1 | Every strategy 76–80% overall; `mixed` within 3 points of revision 10 | gear 76.8, reserve 78.2, honed 78.1, mixed 78.4% (revision 10: 78.6 / 78.2 / 75.0 / 77.8%). `mixed` against revision 10, paired: +0.5 points (−2.2 to +3.7) | **met** |
| D2 | Finale hardest; contract 13's holds | Defeat share by fight 0.0 / 0.0 / 2.0 / 5.0 / **15.8%**; finale minus each other fight +15.8, +15.8, +13.8, +10.8 points (lower bounds +11.7, +11.7, +9.4, +7.1). Rotation 78.3% (76.7–79.7%). Every representative party at least 35% (lowest lower bounds: dup-wizard and support-poor, 45%). Dangerous ÷ Guarded net HP lost 2.41 / 2.52 / 2.27 / 4.84 / 2.10x (lower bounds 2.13 / 2.18 / 1.89 / 3.14 / 1.69; none sign-decided) | **met** |
| C1 | #228 P2: some class has a clear-best card | No class has a card 5 points ahead of both others even on the point estimates. The largest single difference is the Fighter's Honed over Gear, +4.6 points (small). Every other pair is a tie. No extra batch: no class could become clear (contract section 2) | **missed: a design question (below)** |
| C2 | #228 P3: largest card gap ≤ 10 points | gear − honed −1.2 (−3.0 to +0.3), gear − reserve −1.2 (−3.3 to +0.8), honed − reserve 0.0 (−1.6 to +1.7) | **met** |
| C3 | #228 P4: `mixed` rise ≤ 8 points over revision 10, with D2 | +0.8 points in the rotation (−2.2 to +4.1); D2 met | **met** |
| R1 | #187 item 1: quick re-downs at most half the baseline's (≤ 13.6%), and no more fights over 15 rounds than revision 10's 172 | 22.4% of 42,946 rescues (baseline 27.2%); re-downs per rescue 0.65 (0.68). 299 fights over 15 rounds; longest 35 (revision 10: 172; 43) | **missed: a design question (below)** |
| S1 | #187 item 3: fight 5 specials ≥ 1.5x the baseline's (≥ 1.89) | 3.16 per fight (baseline 1.26), 2.5x; by fight 6.34 / 5.15 / 3.22 / 3.01 / 3.16 (baseline 6.23 / 4.54 / 1.71 / 1.38 / 1.26) | **met** |
| T1 | #187 item 4: slot 1 ≤ 28% of damage taken; no slot over 30% of deaths | damage taken by slot 25 / 24 / 22 / 28%; deaths 26 / 25 / 25 / 25%; downs 26 / 22 / 23 / 29% (baseline 33 / 25 / 21 / 21%; deaths 28 / 24 / 24 / 24%; downs 41 / 24 / 19 / 16%) | **met** |
| K1 | #187 item 5: Monk and Paladin closer to the median class, each within 4 points | median 78.3%: Monk 76.4% (−1.9; baseline −3.9), Paladin 79.2% (+1.0; baseline +3.2) | **met** |
| G1 | Gates zero, zero stalls, same seed same results | gates 0 and stalls 0 in all four strategies (12,672 runs); `Project.Balance.SameSeedSameResults` passes | **met** |
| G2 | Revisions 7–10 unchanged | see "Revisions 7–10" below | **met** |

### Other numbers worth knowing

- **Deaths per run** under `mixed` are 0.92 (revision 10: 0.98, baseline 0.63), and fights average 5.92 rounds (revision 10: 6.14, baseline 5.61). The finale's revives per fight are 7.9 (revision 10: 6.7, baseline 6.5): more downs at revision 10's difficulty, rescued with more HP.
- **Class spread.** The rotation class rates run from Sorcerer 73.7% to Fighter 83.5% (baseline: Monk 80.9 to Fighter 90.4). Sorcerer is now the class furthest from the median (−4.6) and Fighter still the strongest (+5.2). Neither was a target.
- **#187 item 2 (melee in the vault),** reported only: the `melee` party wins 96.9% and `ranged` 75.0% (32 runs each). Under the nearest-hero rule, front-liners absorb the hits, so the old melee gap has reversed.
- **Fight 5 specials by strategy:** reserve 3.22, mixed 3.16, honed 2.11, gear 0.22. Gear never refills, so a Gear-only party's specials still fade.

## Misses and the design questions they raise

**C1 (#228 P2): no clear-best card.** Within the levers, the cards came out *more* even (largest gap 1.2 points against M1's 2.7), not more distinct.
- The cause is structural. Contract 13's class rate is a party-policy outcome: all four heroes take the strategy's card. A single card on a single hero moves its party's win rate by about a point, so a class-level difference of 5 points needs one class's card to be several times stronger than any other class's.
- The ticket's per-class levers (per-class Honed rows, a per-class Reserve size) live behind `FBuildCatalog::Abilities(Hero)` and `Caps(Hero)`. Every reader calls those with the hero alone, and revision 10 runs share them, so a revision 11 value needs the run's revision in every reader. That is outside the contract's bounds.
- **Decision needed:** either (a) per-class card rows, with the catalog seams taking the run's revision (a wider rules change), or (b) a measure of "clearly best" that a player's choice reflects: one hero's card, with the other heroes held fixed, which is #228's fourth direction. Option (b) would show per-class differences that the party-wide measure cannot.

**R1 (#187 item 1): the rescue loop is shorter, not gone.**
- Rescue ⅓ cuts quick re-downs from 27.2% to 22.4% and keeps a rescued hero standing more often. It does not halve them.
- Only Rescue ½ does (11.9%), and the party then wins 87% even with every foe at the lever bounds. Restoring revision 10's difficulty from there needs foes beyond the bounds, about 150% HP, which lengthens fights further.
- The long-fight tail is the Dangerous finale. Revision 10's difficulty with the stronger Rescue gives 299 fights over 15 rounds against revision 10's 172; the longest is 35 rounds against 43.
- **Decision needed:** a short post-rescue protection (for example, enemies ignore a hero rescued since its last turn, or a guard until its next turn). It answers the loop directly but needs a new saved per-hero fact, so a **save schema change**, which the spec reserves for Austin. The existing guard fields cannot carry it: restore checks them against the hero's own Protect special and a spent Kit use.

## Revisions 7–10

- **Sweeps.** On the kept build, revision 10 under all four strategies and revisions 7, 8 and 9 under `gear` were swept again. Every `runs.csv`, `encounters.csv` and `heroes.csv` is byte-identical to the same sweep on `50f5dfc2`: 126 files across 7 measurements (`g2_identity.py`). Revision 10 keeps M1's #217 deadlock under `reserve` and `mixed` (Rotation5).
- **Digests.**
  - `Project.Balance.Revision10RunsArePinned` (M1's six revision 10 runs, from unmodified staging `5223a0b4`) still passes.
  - New: `Project.Balance.Revisions7To9RunsArePinned` pins six gear runs of revisions 7–9 (two each, among them rot5-04's seed 20261116). Its digests were captured on `50f5dfc2` before this change. Each digest covers the three CSV rows plus the final snapshot as the real save encodes it, RNG included.
- **SRD** never reads a revision 11 rule (`PlaysRevision11Balance` is Compact only; `Run.Revision11BalanceFollowsTheRevision` checks SRD at revision 11).

## As built

- **`Revision11Rules`** (`Party/BuildCatalog.h`), each value `static_assert`ed within its contract bound:
  - `RescueShare = 3`;
  - `CardsRefill = true`;
  - `EnemyHpPercent = {{100, 100, 100}, {100, 110, 125}}` and `EnemyDamageDelta = {{1, 2, 2}, {2, 2, 2}}` (Guarded, then Dangerous; approach, pressure, finale);
  - Monk `+4` HP and `+1` AC; Paladin `−4` HP and `−1` AC.
- **Gate:** `FExpeditionRun::PlaysRevision11Balance(Ruleset, Revision)` is true for Compact from revision 11. `CardsRefill(Ruleset, Revision)` is true when that gate is on and `Revision11Rules::CardsRefill` is set.
- **Rules that are never saved.** `FCombatEncounter::bNearestTarget` and `bSturdyRescue` are plain members, like M1's `bMoveThrough`. `ApplyRevisionRules` sets all three on the opening encounter, every generated layout and `Restore`.
- **Enemy turn** (`FCombatBattle::PlayEnemyTurn`), with `bNearestTarget`:
  - among the cheapest firing cells it can stop on, it takes the one whose best target has the lowest HP (cell order breaking ties);
  - it attacks the nearest legal target, then the lowest HP, then roster order;
  - without the flag, the older rule applies unchanged.
- **Rescue:** `FCombatBattle::RescueAmount(Target)` is the one amount that resolution, `Forecast` and `FCombatBattle::Describe` (the dock's Rescue text) all read: "Downed ally, 1 cell; restore a third of its max HP".
- **Cards:**
  - `FPartyContinuity::Reward(…, bCardsRefill)`: Reserve refills each pool to its new cap; Honed refills the hero's pools to its caps.
  - `FProgressionQuery` passes the run's flag, so its after-values are the claim's.
  - `FProgressionFacts::bCardsRefill` selects the card rules "+1 cap on each special pool, all refilled" and "Both specials grow stronger; pools refilled". The Honed card also lists the pools it changes.
- **Profiles:** `ApplyRevisionProfile` adjusts each Monk and Paladin hero at `NewRun` and the expected profile in `Restore`, so a revision 11 hero saved with revision 10's numbers is refused.
- **Foes:** `EnemyStats` scales revision 10's result by route and stage kind. Restore regenerates and compares every enemy stat as before.
- **Harness:** `rescues.csv` per group (party, seed, stage, route, outcome, rounds, revives, redowns, quick_redowns). Policy unchanged (`PolicyVersion` 3).
- **Existing tests whose revision 11 outcome changed** (intent kept):
  - `ActionTooltipsMatchHoverAndKeys` expects the battle's own Rescue text (`Describe`);
  - `ReserveBeforeCampRecoversToTheNewCap` plays revision 10 explicitly; revision 11's refill is `Revision11CardsRefillPools`;
  - `PhasesAtomicityRewardsCampAndHistory` checks camp on survivors only, and that the fallen keep their state, because seed 42's first fight now costs a hero;
  - `DeadRosterSurvivesRewardCampAndContinuation` and `PermanentLossContinuesThroughCampAndNextEncounter` take `FirstLossSeed()`, the first seed from 42 whose fixture loses slot 1 and still wins, because enemies no longer attack slot 1 first;
  - `ArtificerRunProgression` takes the first seed its scripted run completes (`FirstSeedWhere`).
- **Save schema 6, generation, rosters and previews are unchanged.**

## Reproduce

```sh
for s in gear reserve honed mixed; do
  DND_BALANCE_REVISION=11 DND_BALANCE_REWARD=$s DND_BALANCE_OUT=.local/balance/m2-final-$s \
    flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock python3 tools/dev.py --cores 2 test --filter Balance.Baseline.
  python3 tools/balance_report.py .local/balance/m2-final-$s
done
python3 -I m2_analysis.py "$PWD" final.json --ref m2-r10 --step m2-final   # from the evidence branch
```
