# Slice 5 contract: progression and replay choices

Status: **child issue [#218](https://github.com/AustinKelsay/dnd-diceroll/issues/218); written 2026-11-12 under [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112)'s delegated authority** (slice 5, "Progression and replay choices").
- Design baseline: staging `5bc0746`. Implementation is **blocked by [#188](https://github.com/AustinKelsay/dnd-diceroll/issues/188)** (PR #214). The fixed review baseline is the staging commit that merges #214, recorded in the implementation claim.
- Measured baseline: the catalog revision 9 run of [12-encounter-identity-report.md](https://github.com/AustinKelsay/dnd-diceroll/blob/feature/188-encounter-identity/docs/runs/gameplay-depth/12-encounter-identity-report.md) (on #188's branch until it merges), with the harness of [07](07-balance-baseline-contract.md), the same 99 parties and the same 32 seeds.
- The before/after evidence will be in `14-progression-report.md`.
- Exact numbers below are **starting values, tuned with the harness and recorded as built** (section 9), as in contracts 09 and 11.

## 1. What the baseline says about progression

- **Levels.** Each of victories 1–4 advances each conscious survivor once, to level 5 for the finale. A level adds +1 basic-attack damage (`FBuildCatalog::BasicDamage`) and nothing else: no HP, refill or new ability.
- **Rewards.** Each of the four ordinary rewards gives one conscious survivor +1 gear rank (+1 basic damage, cap 3) and the route's 1–2 potions. So the only choice is who gets the loot, and every reward is the same +1 damage.
- **Specials fade.** Kit, Rage, Wind, Slot and Maneuver pools are spent by fight 2. Revision 8 used 6.3 / 3.8 / 0.8 / 0.5 / 0.2 specials per fight in fights 1–5 (report 10, target T7, carried to [#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187) item 3). Report 12 does not break this out by fight, so this slice's before-run will.
- **Levels are hard to see.** The reward panel's title says `VICTORY: SURVIVORS GAINED A LEVEL` and the party card shows `LvN`, but the game never shows what a level did.

## 2. Where the choice is offered

The choice comes **at the reward, in place of the fixed gear**. After each of victories 1–4, the player still picks one conscious survivor. That survivor still gets the route's potions, and now also takes **one of three reward cards**:

| Card | Effect on the recipient | Limit |
| --- | --- | --- |
| **Gear** | +1 gear rank (+1 basic damage), exactly as today | rank 3. At rank 3 the card stays legal and gives potions only, as today |
| **Reserve** | +1 cap and +1 use on each pool its specials spend (section 3) | once per hero per run |
| **Honed** | both of its specials improve, each by its effect's row (section 3) | once per hero per run |

Why it works this way:
- **Same count, same moment.** The run still has four rewards, and the reward is still the one point between fights where the player decides. There is no new phase, no extra screen per hero, and no extra stop between fights.
- **A trade.** Each card replaces the +1 gear the reward gives today, so a run gains four upgrades as before. That keeps the power budget near #188's measured balance, and the harness checks it (section 6).
- **Real decisions.** The player chooses between basic damage, more uses of a special, or stronger specials, and chooses which hero carries the upgrade. The cards read the same for all 70 kits, so the choice is quick to learn.
- **No additional draws.** Offers come from state, never from random numbers. A successful claim after fights 2–4 still calls `OfferRoutes`, which draws the next routes from `GenerationRandomState` exactly as today's claim does. The cards add no draw to either stream. Queries, rejected claims and failed commits advance neither stream. So every revision 9 board, encounter seed and combat roll stays as it is.
- **Old habits still work.** The card focus starts on Gear, so the familiar flow (pick a survivor with 1–4, then L) gives exactly today's reward.

Only **Compact runs created under catalog revision 10** offer Reserve and Honed (section 5). SRD runs and older revisions take Gear only, for their whole lifetime.

## 3. Upgrade catalogue

No new specials, passives or slots: the cards change the facts of the specials a kit already has (`FBuildDefinition::Specials`, at most two per hero).

**Honed**, one row per effect or command. Both of a hero's specials take their row.

| Effect / command (kits) | Honed (starting value) | Bound |
| --- | --- | --- |
| Strike, Push (guaranteed damage) | +2 damage | at most +3 |
| Area (each victim) | +1 damage | at most +2 |
| Surge (Wild Bolt, Chaos Bolt) | +1 base amount (outcomes A+1 or 2(A+1)) | at most +2 |
| Magic Missile (Evocation) | +2 damage (3d4+3 → 3d4+5) | at most +3 |
| Healing (Kit specials) | +3 HP | at most +3 |
| Cure Wounds (Life) | +3 HP (2d8+3 → 2d8+6) | at most +3 |
| Second Wind (Fighters) | +3 HP (d10+1 → d10+4) | at most +3 |
| Precision Strike (Battle Master) | +1 accuracy and damage (2 → 3) | at most +2 |
| Protect (self, ally) | +1 reduction | at most +1 (cap 5) |
| Swap | +2 range | at most +2 |
| Rage (Barbarians) | +1 basic damage while raging (2 → 3) | at most +1 |

**Reserve:** +1 cap and +1 use on each pool the hero's specials spend:
- Kit, for every Kit build;
- Kit and Slot for Life and Evocation (their first special spends both);
- Rage for Barbarians;
- Wind for Champion;
- Wind and Maneuver for Battle Master.

Reserve is the only card that adds a use. It is a reward the player chooses in place of gear, like the potions the same reward already gives. It does not refill spent uses: a hero at Kit 0/2 goes to 1/3. Camp's +1 recovery fills up to the new cap. Nothing else gains a use.

### One seam for every reader

ADR-0001 applies: the HUD formats facts and never decides them.
- **Honed** applies inside `FBuildCatalog::Abilities(Hero)`, which returns the honed `Amount`, `Range` and a truthful `Description`. The commands whose numbers are constants rather than ability facts read one catalog function, `FBuildCatalog::HonedBonus(Hero, Command)`.
- **Reserve** comes from a new per-hero cap query, `FBuildCatalog::Caps(Hero)`.
- **Forecast, legality, resolution, status text and presentation all read these two seams.** So a honed forecast always equals the committed result, and the text always matches the rule.

The two inventories below list every reader to move, found in staging `5bc0746` (line numbers are from that commit). PR A reruns both greps on its own base, after #214. Any new hit goes in its as-built section, either moved to the seam or explained.

### Honed consumers

The audit grep is:

```sh
grep -rnE 'GuardAmount > 0|: 3\)|RageTurns > 0|PrecisionBonus|WindDie|FighterLevel|HealingModifier|CureDie|MissileDarts|DartBonus' Source/DnDDiceRoll --include=*.cpp --include=*.h | grep -v -e /Tests/ -e /Rules/
```

A second pass reads the string literals in `Presentation/` for amounts written next to ability text. At `5bc0746` that finds:
- `ExpeditionPlayback.cpp:69`, listed below;
- the reward panel's gear line (`ExpeditionHUD.cpp:1416`), which PR B replaces;
- a fixed roll card in a capture fixture (`ExpeditionCapture.cpp:550`), which is not a rule reader.

Grep hits that are neither readers nor amounts are listed under "Fixed, never honed" or are kit-style tests:
- the `BuildRules` constants themselves (`BuildCatalog.h:22`–`:24`);
- `Abilities` `:212`, the kit-style test;
- the Kit guard's turn rule (`CombatBattle.cpp:117`);
- Rage's own lifetime (`:130`, `:940`, `:956`);
- `ActionFacts.cpp:233` and `ExpeditionHUD.cpp:752`, which test the raging stance.

| Effect | Hard-coded readers that move to the honed seam |
| --- | --- |
| **Shelter** (Totem Warrior, Rage-paid ally Protect, amount 3) | Kit-paid Protect stores its amount in `GuardAmount`. Shelter leaves `GuardAmount` at 0, so every reader falls back to a literal 3:<br>- `ProtectionFor` (`CombatBattle.cpp:961`), which is the actual reduction and the source of `FCombatBattle::Protection`;<br>- the committed protect effect (`:698`, `A.Amount > 0 ? A.Amount : 3`);<br>- `FBuildCatalog::Status` (`BuildCatalog.cpp:233`);<br>- the hero-details protection line (`ExpeditionHUD.cpp:811`). |
| **Rage** | the +2 basic damage in `FBuildCatalog::AttackModifier` (`BuildCatalog.cpp:239`);<br>the catalog text "+2 basic damage, reduce damage by 1";<br>the playback preview "Rage: +2 basic damage, -1 taken" (`ExpeditionPlayback.cpp:69`). |
| **Second Wind** | `WindDie`/`FighterLevel` in `DescribeAction` (`CombatBattle.cpp:233`), the forecast (`:541`) and resolution (`:825`). |
| **Cure Wounds** | `HealingModifier` in `DescribeAction` (`:235`), the forecast (`:489`, `:542`) and both resolution paths (`:665` catalog, `:826` fixture);<br>the Life description "heal 2d8+3" (`BuildCatalog.cpp:159`). |
| **Magic Missile** | `MissileDarts * DartBonus` in `DescribeAction` (`:237`), the forecast (`:540`) and resolution (`:797`–`:812`), plus its feedback (`:819`);<br>the Evocation description "guaranteed 3d4+3". |
| **Precision Strike** | `PrecisionBonus` in accuracy (`:456`), forecast damage (`:535`) and resolution (`:708`). |
| **Kit abilities** (Strike, Push, Area, Surge, Healing, Protect, Swap) | already read `Abilities(Hero)`:<br>- legality (`:338`–`:344`) and range (`:399`);<br>- forecast (`:472`–`:495`);<br>- resolution (`:651`–`:700`);<br>- protection validation (`:123`);<br>- the playback preview (`ExpeditionPlayback.cpp:71`).<br>Their descriptions come from the catalog's `Ability` formatter, which re-runs with honed values. |
| **Barbarian specials' descriptions** (Frenzy, Shelter, Long Claw, Wild Bolt) | hand-written before the `Ability` formatter runs (`BuildCatalog.cpp:45`–`:48`): Frenzy "guaranteed 4 damage", Shelter "reduce damage by 3 until next own turn", Long Claw "guaranteed 6 damage", Wild Bolt "4 damage or 2: 8 damage". Their numeric facts hone, but these strings do not. PR A regenerates all four from the (honed) facts. Shelter's text states its real lifetime in the ability description ("reduce damage by N until its next turn or its Rage ends") as well as in status and hero details ("until next own turn or Rage ends"): like every guard it ends when its Barbarian's next own turn starts (`AdvanceTurn`), and sooner when that Rage ends (`:940`). *Corrected in #218 PR A with the lead's approval: this row first said "while its Barbarian rages", which the unchanged lifetime rule does not do.* The all-kit description check covers their numbers and Shelter's lifetime. |

**Shelter keeps its Rage lifetime.** The guard still ends when its Barbarian's Rage ends (`CombatBattle.cpp:940`), and the restore rule for a Rage guard with no `GuardTurns` (`:132`) is unchanged.
- **Amount.** One function, `FBuildCatalog::GuardAmount(Guard)`, gives the amount: `GuardAmount` when the guard is Kit-paid, otherwise the guard's own Rage-paid Protect fact, honed when `bHoned` is set. The four readers above call it. No new saved field is needed: `bHoned` cannot change during a battle, because claims only happen between fights. So a checkpoint saved with Shelter active resumes with the same amount.
- **Text.** The status and hero-details text say the guard lasts while raging, not "until next own turn".

**Fixed, never honed** (listed so that a reviewer can tell an omission from a decision):
- the Potion's 2d4+2 and Rescue's 1 HP;
- `TouchRange` for Potion and Rescue, and Magic Missile's range 6;
- the Sneak Attack die;
- Rage's −1 incoming damage and its two-turn duration (the `2` in the Rage effect at `:698` is a turn count).

### Cap consumers

The audit grep is:

```sh
grep -rnE 'KitCap|SlotCap|WindCap|ManeuverCap|RageCap|ResourceCap|> 2 \|\||SecondWindUses > 1' Source/DnDDiceRoll --include=*.cpp --include=*.h
```

| Reader | What moves to `Caps(Hero)` |
| --- | --- |
| Combat validation, `CombatBattle.cpp` | `ManeuverUses > 2` (`:79`); `SecondWindUses > 2` and `SpellSlots > 2` (`:101`); `KitUses > KitCap` and `GuardAmount > 4`, which becomes the honed Protect maximum (`:116`); the untouched Kit pool (`:118`); `RageUses > RageCap` (`:128`); the untouched Rage pool (`:130`). In the bonus-budget rule, `KitUses >= KitCap` and `SecondWindUses > 1` (`:1209`–`:1213`) mean "untouched", so they become `== Caps(Hero)`. |
| Catalog, `BuildCatalog.cpp` | `ValidResources` (`:192`); `SurgeCap` (the `KitUses < KitCap` and `RageUses < RageCap` tests). |
| Facts, `ActionFacts.cpp` | `ResourceCost` caps (`:37`–`:41`), which the usage tags and tooltips read. |
| Camp, `PartyContinuity.cpp` | `Recover` (the `B->XCap` minimums). The fixture-only `ResourceCap = 2` path keeps its meaning for heroes with no build. |
| Camp text | `RecoveryDescription` (`:58`–`:63`) prints `caps: max HP/2`, and the camp panel shows that string. Because Reserve at reward 1 comes just before the one camp, the text becomes `Survivors: +10 HP, +1 Wind/slot/Rage/Kit up to each hero's own cap. No revival or potions.`, which is true for every ruleset and revision. In PR B the camp panel also lists each survivor's before → after values from `FProgressionQuery` (`Kit 1/3 → 2/3`). |
| HUD, `ExpeditionHUD.cpp` | the party-card resource groups (`:745`–`:749`). |
| Harness, `BalanceHarness.cpp` | `kit_cap` … `rage_cap` are read from the build at run start (`:612`–`:613`). They become `Caps(Hero)` at the end of the run, so the exported caps agree with play. |

These stay on `FBuildDefinition`, because they are not live per-hero caps:
- the catalog's base-cap definitions and camp recovery amounts;
- the kit-style tests (`KitCap > 0` or `B->KitCap` meaning "a Kit build"): `Abilities` `:212`, `Status` `:228`–`:232`, `ActionFacts::IsRageStyle` `:50`, and validation `:111` and `:118`. `:118` reads both a style test and an untouched pool; only the pool moves;
- the catalog's definitions of caps and recoveries (`BuildCatalog.cpp:22`–`:68`, `:137`, `:159`, and `BuildCatalog.h:10`–`:14`);
- fixtures in tests that build unupgraded heroes.

## 4. Level gains made visible

Levels keep today's rule and effect (+1 basic damage, no HP or refill). A new const query, `FProgressionQuery::For(const FExpeditionRun&)`, reports what presentation shows. It uses no RNG and is derived only from saved state:
- **per hero:** level, the level before the last victory, whether it levelled, basic damage and its parts (build base, levels, gear, Rage), gear rank, Reserve, Honed, and the per-hero caps;
- **in the Reward phase, per survivor:** the three cards, each with availability, a precise blocker (`Already honed`, `Reserve taken`, `Gear at rank 3: potions only`, `Fallen`, `Gear only in this run`), and before → after values (basic damage 6 → 7; `Kit 1/2 → 2/3`; `Volley 4 → 5 · Hunter's Finish 4 → 6`);
- **in the Recovery phase, per survivor:** HP and every pool before → after the camp, each at the hero's own cap.

Presentation (PR B, section 8) shows level gains in four places:
- **The reward panel title:** `VICTORY 2 OF 5 · SURVIVORS REACH LEVEL 3`.
- **Each party card:** a `+1 Lv` marker, with `basic 5 → 6`.
- **Fallen heroes:** `fell at Lv2`.
- **The hero details:** `Basic damage 6 = 3 build + 2 levels + 1 gear` and `Upgrades: Reserve, Honed`.

The battle log gains one line at each of victories 1–4: `Level 3: +1 basic damage (Aria, Bram, Cass)`.

## 5. Commands, persistence and versioning

**Command.** `FExpeditionRun::ClaimReward(int32 RecipientId, ERewardCard Card, FString& Error)`, with `ERewardCard { Gear = 0, Reserve = 1, Honed = 2 }`. The session mirrors it. The existing `ClaimReward(Id, Error)` stays as the Gear overload for existing callers and tests.
- The claim builds a candidate, and the session commits the checkpoint before installing it (the existing `Commit`). So a failed write rejects the claim and leaves the state, the RNG and the file bytes as they were.
- **Rejections are named, and leave the full snapshot unchanged:** wrong phase; recipient not a conscious survivor (existing); `Gear only in this run` (SRD, or revision 7–9); `Reserve taken`; `Already honed`; a card value out of range.

**Saved state** (save schema 6):
- `FExpeditionEncounterRecord::RewardCard`: the authoritative record of each claim.
- `FCombatantState::bReserve` and `bHoned`: copies of that record, which combat and the catalog read. Enemies and SRD heroes always have both false.

**Detached restore validates every derived fact against history** (as `WeaponRank` is today):
- `WeaponRank = min(3, the hero's Gear claims)`;
- `bReserve` and `bHoned` are true exactly when history holds that claim for that hero, and history holds at most one of each per hero;
- non-Gear claims appear only on rewarded records of Compact revision 10 runs. A record with no reward holds `Gear`;
- every pool fits within `Caps(Hero)`, and a live guard matches its honed Protect amount;
- dead heroes keep their upgrades and their death level;
- no card changes `MaxHP`, HP or any other profile field.

**Catalog revision 10.**
- New runs are created under revision 10, and restore accepts 7 to 10.
- Revisions 7–9 keep their generation, enemies, previews and gear-only rewards for life, exactly as #186 and #188 did.
- Revision 10's **board generation and RNG** are revision 9's exactly: the same recipes, sizes, encounter seeds, rosters, enemy stats and draws in the same order. This slice changes rewards, not encounters.
- Unknown revisions are still refused by name and preserved. `NewRun` keeps its explicit older revision for tests and the harness.

**Route reward previews.** #188's `DescribeRoute` promises `+1 gear, +N potion(s)`, which a revision 10 Compact run no longer guarantees. So the reward line becomes revision-aware, and this is the one deliberate text change:

| Run | Reward line on routes 1–4 | Finale |
| --- | --- | --- |
| Compact, revision 10 | `1 upgrade choice, +N potion(s)`, keeping ` (secured supplies)` | `Recover the artifact; no loot`, unchanged |
| Compact, revision 9 | `+1 gear, +N potion(s)` (#188's wording, unchanged) | unchanged |
| Compact, revisions 7–8, and every SRD run | today's wording for that run, unchanged byte for byte | unchanged |

Restore already rebuilds each saved route with `DescribeRoute` and compares the whole struct. It now passes the run's revision and ruleset, so each run validates against its own wording. Comparisons between revisions 9 and 10 exclude the `Preview` strings and nothing else (section 7, `GearOnlyRevision10MatchesRevision9`).

**Save schema 6.**
- Staging and #188 are both on schema 5 (`FExpeditionSave::Version`, from #60).
- The decoder becomes explicit step migrations, 4 → 5 (existing) and 5 → 6 (new: every record gets `RewardCard = Gear`, every combatant `bReserve = bHoned = false`). That is exactly what a schema 5 run means.
- Schema 6 documents import strictly: no field injection for the current version, and the version must be an exact supported integer.
- Unknown or future versions are refused and stay discoverable at `current-v1.json`, and startup New Run preserves them beside it, as today.

**Interaction with #95 (SRD).**
- **Schema numbers.** #95's plan named "PR 3" as the delivery that introduces schema 5. Schema 5 already shipped on staging (ruleset choice, #60). Landon's parked PR #177 branch writes schemas 6 and 7 for SRD XP, class levels and pact slots. Those are not on staging, and the [2026-11-12 maintainer direction](https://github.com/AustinKelsay/dnd-diceroll/pull/177) moves them to a later #95 PR.
- **The rule: a schema number is taken when its PR merges.** Slice 5 is built as schema 6. If a #95 schema change merges first, slice 5 takes the next free number. Either way, the second PR to merge adds one step migration and a real fixture of the first one's format. The same rule applies to catalog revision numbers.
- **Level-ups.** #95 PR 5 brings SRD advancement (XP, class levels, an SRD Level Up between battles). This slice is Compact only: SRD rewards stay gear-only, and the two new flags stay false in SRD. SRD's level-up is its own command and shares no field with the reward cards.
- **Heads-up.** The implementer posts a heads-up on #95 before PR A merges, as #186 and #188 did.

## 6. Harness measurement plan

**Policy version 2** is version 1 plus one reward rule. With the `gear` strategy it is version 1 exactly. The strategy comes from `DND_BALANCE_REWARD`:
- `gear`: version 1, which gives Gear to the survivor with the lowest gear rank, then the lowest ID.
- `reserve` and `honed`: that card when it is available, otherwise Gear. The recipient is the survivor with the fewest upgrades (gear rank + Reserve + Honed), then the lowest ID. Under gear-only play that is version 1's rule.
- `mixed` (the default): the same recipient as `reserve` and `honed`. Reserve when each of the recipient's special pools holds at most one use (per pool, not summed, as the claim sees them: after the fight, before the camp) and Reserve is free; otherwise Honed when it is free; otherwise Gear. *(Clarified in #218 PR A with the lead's approval.)*

**Runs.** Each run is a full `Balance.Baseline.` under the machine lock at `--cores 2`:
- **Before:** revision 9 on this build (`DND_BALANCE_REVISION=9 DND_BALANCE_REWARD=gear`; a revision 9 run can only take Gear).
- **After:** revision 10 under each of `gear`, `reserve`, `honed` and `mixed`.

That is five measurements of 3,168 runs each, about 45 s of locked wall time apiece. The baseline is #214 as merged: if #214's pending soft-lock revision changes report 12's numbers, the before-run is compared against the after-run that #214 finally retains.

**Harness additions.** Columns are only ever appended, after the existing ones:
- `heroes.csv` gains `reward_cards` (in claim order, `G`, `R` or `H` joined by `;`, empty when none), `reserve` (0/1) and `honed` (0/1). The existing `kit_cap` … `rage_cap` columns are now read from `Caps(Hero)` at the end of the run.
- `runs.csv` and `encounters.csv` gain no columns. Specials per fight by stage already come from `encounters.csv` (`stage`, `specials_used`).
- `group.json` gains `reward_strategy`. `policy_version` becomes 2.
- `tools/balance_report.py` gains:
  - the strategy tables and paired differences below;
  - specials per fight by stage;
  - a `compare` command that implements the projections below and exits non-zero on any difference. It has helper tests for match, mismatch, missing rows and extra rows.

### CSV comparisons

**Row keys:**
- `runs.csv` by (`party`, `seed`);
- `encounters.csv` by (`party`, `seed`, `stage`);
- `heroes.csv` by (`party`, `seed`, `slot`).

Row order is not compared, but the key sets must be equal: a missing or extra row fails.

**C1, before-run against #214's retained revision 9 after-run** (this proves the revision 9 path is untouched):
- Every column in #214's header for each file must be equal, with values compared as exact strings.
- Excluded: only the columns this slice appends (`reward_cards`, `reserve`, `honed`). Those are checked separately: every before-run row must hold `G` repeated once per reward claimed, `0` and `0`.
- `group.json`: equal party list, seeds, run count and `catalog_revision`. Excluded: the elapsed time, `policy_version` (1 → 2) and the new `reward_strategy`.

**C2 (target P1), revision 10 `gear` against the before-run:** both come from this build, so every column of all three CSVs must be equal, the new columns included. That covers outcomes, rounds, damage, healing, protection, resources left and their caps, potions, commands and blockers. `group.json` differs only in `catalog_revision` (9 → 10) and the elapsed time. `reward_strategy` is `gear` in both.

Neither comparison has an ad hoc exclusion: a column that differs and is not listed above fails the check.

**New fields under card strategies** are validated on their own:
- for each hero, `reward_cards` matches the reward recipients in history: one card per claim, at most one `R` and one `H`, and gear rank = min(3, the number of `G`);
- `reserve` and `honed` agree with the cards;
- the end-of-run caps equal the build caps plus `reserve`.

### Statistics

Every strategy plays the same 99 parties on the same 32 seeds and route patterns, so outcomes are **paired by (party, seed)**. A pair is identical until the first claim in which the strategies differ.

**Counting:**
- A run is one (party, seed).
- A class's win rate in the rotation is over the runs of the rotation parties that contain at least one hero of that class. Each run counts once for a class, even with duplicates.
- Representative parties are reported separately and not pooled into class rates.
- A class win rate is a **party-policy outcome**: the strategy applies to every claim the party makes. It is not the causal value of one card to that class, and the report says so.

**Paired difference.** For strategies A and B over a set of runs, d is the mean of (win_A − win_B), in points. Its 95% interval is a paired bootstrap:
- resample the seeds with replacement, keeping every party of a drawn seed together (seeds are the shared unit);
- 10,000 resamples, from a fixed bootstrap seed `20261112`, with Python's `random.Random`;
- take the percentile interval.

The same method gives intervals for the specials-per-fight ratio (P5) and the rotation gaps (P3, P4).

**Verdicts for one difference:**
- **clear** when the interval excludes 0 and |d| ≥ 5 points;
- **small** when the interval excludes 0 but |d| < 5;
- **tie** when the interval includes 0.

A strategy is the **best for a class** only when it is clear against both other single-card strategies there. Otherwise the class is recorded as a tie (or small) and counts for no strategy.

**Inconclusive.** A threshold target is inconclusive when its interval straddles the threshold:
- P3, a gap whose interval contains 10;
- P4, a rise whose interval contains 8;
- P5, a ratio whose interval contains 1.5.

P2 is inconclusive when it would be met if every small or tied class it needs went its way, but is not met on clear classes alone.

Rules:
- The slice 3/4 holds in P4 each use a statistic suited to their kind:
  - **win-rate holds** (rotation 55–90%, every representative party at least 35%): the point estimate with its Wilson interval, as in contracts 09 and 11;
  - **Dangerous at least 1.5x Guarded HP**: the metric inherited unchanged from contract 09 T4 and report 12. For **each fight 1–5**, take the mean **net** HP lost per fight on Dangerous routes divided by the same mean on Guarded routes (report 12: 1.6x at fight 5 to 3.3x at fight 4). Each fight's ratio gets a seed-cluster bootstrap interval, using the same fixed resample seed and resampling whole seeds. The hold is **met only when every fight's lower bound is at least 1.5**. A pooled total never stands in for it, because a strong fight can hide a failing one. Net HP lost is signed (healing can exceed damage), so the ratio is defined only for a positive Guarded mean. One sign-safe rule, `D` and `G` being the Dangerous and Guarded means, decides both the full sample and every resample:
    - **above the threshold** only when `D > 0` and either `G <= 0`, or `G > 0` and `D / G >= 1.5`;
    - **below it** in every other case. Any `D <= 0` is below, including both means negative, where a ratio such as −20% / −10% = 2.0 must not pass.
    - The interval is taken over the per-resample ratio when `G > 0`. The report also counts, per fight, the resamples decided by the sign rule instead of the ratio, and the lower bound is the 2.5th percentile of the resample outcomes with sign-decided ones placed at +inf (above) or −inf (below).
    - The helper tests add fixtures for `G = 0, D > 0`; `G = 0, D = 0`; `G < 0, D < 0`; `G > 0, D < 0`; and `G < 0, D > 0`.
    A fight with no Guarded or no Dangerous encounters in the full sample is reported as **not evaluable (not met)**.
  - **finale has the highest defeat share**: for each other stage, the bootstrap interval of (finale share − that stage's share), with shares pooled over routes and a seed-cluster bootstrap. The hold is met when every difference is above 0.
  A hold whose interval straddles its threshold is marked **borderline**.
- If any of P2–P5 is inconclusive, or a P4 target is borderline, the report adds **exactly one extra fixed-seed batch**. That batch is seeds `20261141 + i` for `i = 0 … 31`, with the same parties and the same route-pattern rule, for all five measurements. Every verdict is then made on the 64 pooled seeds.
- A target still inconclusive after the extra batch, or a P4 hold still borderline after it, is reported as **inconclusive (not met)** with its intervals. A borderline hold is never passed on its point estimate. There are no further batches, and stopping is not optional.
- `tools/balance_report.py` implements these statistics, and its helper tests cover a met, a missed and a borderline case for each hold kind, plus a zero-denominator resample, and a case where a pooled ratio passes while one fight fails (the per-fight rule must report it missed).

**Dominance.** Meeting P2 on the fixed sample does not establish by itself that "no card strictly dominates". The report claims non-dominance only when every one of `gear`, `reserve` and `honed` has at least one class where another strategy is clear against it. Otherwise it says which strategy was not beaten anywhere.

### Targets

These are design proposals, judged met, missed or inconclusive against the evidence under the rules above:

| # | Target | Why |
| --- | --- | --- |
| P1 | **Gear is revision 9:** comparison C2 passes | the choice is opt-in and changes nothing else |
| P2 | **Choices are real:** each of `gear`, `reserve` and `honed` is the best (clear against both others) for at least two of the 13 classes in the rotation | every card has a use |
| P3 | **No trap card:** the largest paired gap between the rotation win rates of `gear`, `reserve` and `honed` is at most 10 points | every card is a sensible pick |
| P4 | **Bounded power:** the paired rise of `mixed` over the before-run in rotation win rate is at most 8 points, and the slice 3/4 targets #188 met still hold (rotation 55–90%, every representative party at least 35%, finale highest defeat share, Dangerous at least 1.5x Guarded HP) | no power creep; do not undo slices 3–4 |
| P5 | **Specials reach the finale:** under `reserve`, specials per fight in fights 4–5 are at least 1.5x the before-run's | #187 item 3, for the player who chooses it |
| P6 | Gates zero (policy errors, illegal proposals, passive ends, protection mismatches), same seed same results, and no more stalls than the before-run | contract 07 |

**Levers**, smallest first: the Honed amounts and Reserve's +1, within the bounds of section 3. **Enemy numbers and catalog base numbers are not levers here.**
- If P3 or P4 cannot be met within the bounds, the report says missed and records a follow-up.
- Every lever step reruns all five measurements on the base 32 seeds. The extra batch is used only for the kept step's verdicts.
- #188's two misses (victories that lose a hero, and the melee party's gap) are reported, not targeted.

## 7. Tests (public seams; each red before green)

**Run and catalog** (`Project.Expedition.`):
- `Run.RewardCardsApplyOnce`: each card's effect, limit and potion supply on a revision 10 run; Gear at rank 3 gives potions only.
- `Run.RewardCardRejectionsLeaveStateUnchanged`: every rejection above, with full snapshot and RNG equality.
- `Run.GearOnlyRevision10MatchesRevision9`: a revision 10 run claiming Gear at every reward, compared with a revision 9 run of the same seed and party, command for command through Victory or Defeat. Every snapshot field is equal (boards, encounter seeds, recipes, sizes, enemies, party, history, both RNG states, `RewardCard`, `bReserve`, `bHoned`) except two: `CatalogRevision`, and the `Preview` strings of offers and history. For those strings, the only difference is that the reward phrase reads `1 upgrade choice` instead of `+1 gear`.
- `Run.RewardPreviewsFollowRevision`:
  - revision 10 Compact offers say `1 upgrade choice, +N potion(s)`, with secured supplies kept;
  - revision 9, revisions 7–8, SRD and every finale preview are byte-identical to the base;
  - restore accepts each run's own wording, and rejects revision 9 wording on a revision 10 run and revision 10 wording on a revision 9 or SRD run.
- `Run.OlderRevisionsAndSrdRewardGearOnly`.
- `Run.ReserveBeforeCampRecoversToTheNewCap`: Reserve at reward 1 on a hero with a spent pool, then camp, for Kit, Kit + Slot, Rage and Wind + Maneuver builds. Only one hero receives the first reward, so each build is its own run (fixed seed and party, with the Reserve recipient varied). Each pool goes to `min(cap + 1, held + 1 + recovery)`, where `recovery` is that pool's existing camp recovery: Wind's +1, Maneuver's 0 (`ManeuverRecovery` defaults to 0 and Battle Master never sets it, so a spent Maneuver pool goes 0/2 → 1/3 on Reserve and stays 1/3 after camp), and each Kit, Slot and Rage pool's own value. Heroes without Reserve stop at their own cap, and no pool's recovery amount changes. `RecoveryDescription` says recovery stops at each hero's own cap.
- `Run.LevelGainsAreFactsOnly`: `FProgressionQuery` values; levels change no HP or pool; dead heroes keep their death level and upgrades.
- `Catalog.HonedFactsMatchForecastAndResolution`: all 70 kits, every honed special. Facts equal forecast equals the committed result, and the description is truthful. For Protect and Rage, the check goes beyond damage and healing fields: the committed protect effect, `FCombatBattle::Protection` (amount and source) and the `DamagePrevented` of a real enemy hit all equal the honed amount, and Rage's basic-damage forecast and hit use the honed bonus.
- `Catalog.HonedShelterProtectsThroughRage`: a Totem Warrior that claimed Honed rages and casts Shelter on an ally. Then:
  - `Protection(ally)` is 4 and names the Barbarian;
  - an actual enemy hit on the ally prevents 4;
  - the status and hero-details text show 4 and the Rage lifetime;
  - a checkpoint written with Shelter active resumes with the same protection and continues identically;
  - the guard ends when Rage ends, as before;
  - an unhoned Totem Warrior still prevents exactly 3.
- `Catalog.ReserveCapsEverywhere`: per-hero caps in facts, validation, camp and usage tags, for Kit, Kit + Slot, Rage, Wind and Wind + Maneuver builds.

**Persistence:**
- `Persistence.Schema5LoadsAsGearOnly`: **real schema 5 fixtures** written by unmodified staging after #214 (revision 8 and 9; route, mid-battle and reward phase) under `Tests/Fixtures/Schema5/`, with a README as for Schema4. Each resumes and continues exactly as staging's own continuation.
- `Persistence.UpgradeChoicesRoundTrip`: Reserve and Honed picks through write, read and Resume in every phase, continuing identically.
- `Persistence.RestoreRejectsUnearnedUpgrades`: flags without history; history without flags; two Reserve claims; a non-Gear claim on revision 9 or SRD; pools above `Caps`; a route preview in the wrong revision's wording; a schema 6 document missing a field; a fractional version; version 7 refused, then preserved by New Run.
- `Persistence.FailedUpgradeClaimIsAtomic`: a failed write leaves the live state, the RNG and the file bytes unchanged.

**Presentation (PR B):**
- `Presentation.RewardCardsShowProgressionFacts`: card text equals the query, including every blocker.
- `Presentation.CampTextUsesEachHerosCap`: the camp panel's text and per-survivor lines equal the query in a mixed party where one hero took Reserve at reward 1.
- `PartyDetailsKeepClearOfPhasePanels`: run with the new reward panel size.
- An input-harness test: the keyboard picks a card and claims it. Hints and Help do not leak input, and Return to menu at the reward writes nothing.

**Balance:**
- `Project.Balance.RewardStrategiesSubmitOnlyLegalClaims`;
- `Project.Balance.GearStrategyIsPolicyOne`: comparison C2 on two parties and two seeds;
- `Project.Balance.ExportedCapsMatchPlay`: `kit_cap` … `rage_cap` equal `Caps(Hero)` for Reserve heroes.

Helper tests cover `tools/balance_report.py`:
- `compare` (keys, projections, missing and extra rows);
- the paired seed bootstrap (a fixed bootstrap seed gives a fixed interval);
- the clear / small / tie and inconclusive verdicts, and the best-for-class rule with ties.

**Kept, with intent preserved:**
- `Schema4LoadsAsCompact` and `OlderRevisionsResumeWithTheirOwnEnemies`, unchanged;
- `Schema4UpgradeRoundTripsCurrentRuns`, extended to revision 10;
- `PhasesAtomicityRewardsCampAndHistory`, `FailedMilestoneIsAtomic`, `SubclassAndMilestoneCheckpoint`, `DeathLevelRespectsEarnedHistory`, and every `*RunProgression` and `*SpentPoolsAcrossProgression` test;
- `CompactRevision9SeedFingerprint`, which creates its revision 9 run explicitly.

A full-run test whose seed changes outcome under revision 10 takes `FirstSeedWhere`, as #184 and #188 did. Each such fixture change is recorded in the PR.

## 8. Presentation needs (PR B)

**Reward panel:**
- the level title and the potion line;
- three cards for the selected survivor (Gear, Reserve, Honed), each with before → after values from the query. Unavailable cards are dimmed and show their blocker;
- the Claim button names both card and hero (`Give Reserve to Aria`).

**Input:**
- 1–4 or a click picks the survivor, as today;
- a click, Left/Right or Tab, or a letter key picks a card. The letter keys are chosen against the bindings live in the Reward phase and listed in Help;
- L or Enter claims;
- focus starts on Gear at the start of **every** Reward phase and after Resume. The card focus is presentation state and is not saved;
- card selection keeps the existing overlay and focus gating: no card input while Help, Records, a hint or the Return-to-menu confirmation is open, and Tab keeps its per-screen focus order (#129).

**Elsewhere:**
- the `+1 Lv` markers, hero details and the battle-log line (section 4);
- the action dock's usage tags and tooltips use the per-hero caps and honed amounts in the next fight (`Kit 3/3`, `Volley 5`), and so do the party-card resource groups;
- the camp panel shows the new `RecoveryDescription` and each survivor's before → after values from the query;
- route cards show the revision-aware reward line;
- `RewardPanelRect` grows to fit the cards, and #200's placement rule must still hold.

**Captures** (`tools/capture/`, a `reward` pose set; gallery fixtures `reward-choice-*`):
- each card focused;
- Gear at rank 3;
- every card taken;
- Reserve on a Life cleric (Kit + Slot), a Barbarian (Rage) and a Battle Master (Wind + Maneuver);
- a fallen hero;
- the camp after Reserve at reward 1;
- a route card under revision 10;
- each at 1280x720, 1920x1080 and 1600x600, inspected.

**On-screen pass** on a packaged build before PR B merges, with a real keyboard and pointer:
- claim each card;
- take Reserve at reward 1 and check the camp text and values;
- see the dock, tooltips and party cards change in the next fight, including a honed Shelter;
- Resume at the reward and just after a claim;
- Return to menu at the reward;
- Help;
- the limits recorded honestly.

No new assets. A reward-claim sound cue belongs to slice 6.

## 9. Delivery and as built

- **PR A (rules, no `Presentation/`, `Content/` or `Config/`):**
  - cards, catalogue and the two seams, with both inventories moved (section 3) and the rerun greps recorded;
  - the revision-aware previews and the new `RecoveryDescription`;
  - `FProgressionQuery`, revision 10, and schema 6 with its fixtures;
  - harness policy 2, the `compare` command and the statistics, plus `14-progression-report.md`;
  - updates to the rules reference, the glossary (Reward card, Gear, Reserve, Honed, Progression facts) and current state.

  Because the presentation readers (`ExpeditionHUD.cpp:745`–`:749` and `:811`, `ExpeditionPlayback.cpp:69`) live in `Presentation/`, they move in PR B. Until then, the game's only claim path is the Gear overload (the HUD's `Loot`), so no player can hold a honed or Reserve hero that these readers would misdescribe. PR B must not merge without them.
- **PR B (stacked on A):** section 8.
- **Gates for each PR:** build with 0 warnings, full `Project.` with 0 warnings, helpers on Python 3.11, `contribute.py check`, and two-axis review. PR A also needs the `Balance.Baseline.` before and after runs and an evidence branch. PR B also needs inspected captures and the on-screen pass before merge.
- **As built:** filled in by PR A (numbers, deviations, targets met or missed) and PR B (presentation), as in contracts 09 and 11.
  - **PR A ([#218](https://github.com/AustinKelsay/dnd-diceroll/issues/218)):** see [14-progression-report.md](14-progression-report.md), "As built" and "Targets". Honed rows are at their bounds (Strike/Push +3, Area +2, Surge +2, Missile +3, Precision +2; the others as written). P1, P3, P4 and P5 are met. P2 is missed (accepted by the lead on 2026-11-13; follow-up #228). P6 is missed on one #217 deadlock, accepted for the release by the maintainer on 2026-11-13. Two lead-approved corrections are in this file: Shelter's lifetime text (section 3) and the `mixed` rule (section 6).

## 10. Non-goals

- New specials, passives, a third special slot, or changes to any kit's base numbers in the shared catalog.
- HP growth, refills or new abilities on levelling; a level cap above 5; multiclassing; XP.
- Respec, undoing a claim, or picks for Downed or dead heroes; healing or revival from any card.
- Random or rerolled offers, or shops.
- Upgrades or unlocks that carry across runs. Run records (`FExpeditionEnding`) are unchanged.
- SRD-mode upgrades (#95 PR 5).
- Enemy stats, rosters or behaviour; #187's rescue loop, target choice and Monk items; #188's body-blocked stalemates.
- Audio cues (slice 6).
