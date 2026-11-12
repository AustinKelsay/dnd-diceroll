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
- **No hidden draws.** Offers come from state, never from random numbers. So `GenerationRandomState`, combat RNG and every revision 9 board stay as they are, and a rejected or failed claim cannot change RNG.
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

**One seam for every reader** (ADR-0001: the HUD formats facts and never decides):
- **Honed** applies inside `FBuildCatalog::Abilities(Hero)`. It returns the honed `Amount`, `Range` and a truthful `Description`. The constant-based commands (Second Wind, Precision Strike, Magic Missile, Cure Wounds, Rage) read one catalog function for their bonus.
- **Reserve** comes from a new per-hero cap query, `FBuildCatalog::Caps(Hero)`. It replaces every direct read of `FBuildDefinition::{Kit,Slot,Wind,Maneuver,Rage}Cap` and the hard-coded caps in combat validation (`ManeuverUses > 2`, `SecondWindUses > 2`, `SpellSlots > 2`, `GuardAmount > 4`, the untouched-pool checks of the bonus-budget rule). That covers combat validation, `ValidResources`, `SurgeCap`, `FActionFactsQuery` caps and usage tags, `FPartyContinuity::Recover` and the HUD resource groups.
- **Forecast, legality and resolution read the same facts**, so a honed forecast always equals the committed result.

## 4. Level gains made visible

Levels keep today's rule and effect (+1 basic damage, no HP or refill). A new const query, `FProgressionQuery::For(const FExpeditionRun&)`, reports what presentation shows. It uses no RNG and is derived only from saved state:
- **per hero:** level, the level before the last victory, whether it levelled, basic damage and its parts (build base, levels, gear, Rage), gear rank, Reserve, Honed, and the per-hero caps;
- **in the Reward phase, per survivor:** the three cards, each with availability, a precise blocker (`Already honed`, `Reserve taken`, `Gear at rank 3: potions only`, `Fallen`, `Gear only in this run`), and before → after values (basic damage 6 → 7; `Kit 1/2 → 2/3`; `Volley 4 → 5 · Hunter's Finish 4 → 6`).

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

**Catalog revision 10.** New runs are created under revision 10, and restore accepts 7 to 10. Revisions 7–9 keep their generation, enemies, previews and gear-only rewards for life, exactly as #186 and #188 did. Revision 10's generation is revision 9's exactly: this slice changes rewards, not encounters. Unknown revisions are still refused by name and preserved. `NewRun` keeps its explicit older revision for tests and the harness.

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
- `mixed` (the default): Reserve when the recipient's special pools hold at most one use and Reserve is free; otherwise Honed when it is free; otherwise Gear.

**Runs**, each a full `Balance.Baseline.` under the machine lock at `--cores 2`:
- **Before:** revision 9 on this build (`DND_BALANCE_REVISION=9`). Its CSVs must equal #188's kept after-run, which proves the revision 9 path is untouched.
- **After:** revision 10 under each of `gear`, `reserve`, `honed` and `mixed`.

Five measurements of 3,168 runs each, about 45 s of locked wall time apiece.

**Harness additions:**
- per hero: `reward_cards` (for example `G,R,H,G`) and the per-hero caps at the end of the run;
- per encounter: specials used by stage;
- in `group.json`: `reward_strategy` and `policy_version`;
- in `tools/balance_report.py`: a strategy table, the best strategy by class, and specials per fight by stage.

**Targets** (design proposals, judged met or missed against the evidence):

| # | Target | Why |
| --- | --- | --- |
| P1 | **Gear is revision 9:** under `gear`, the revision 10 CSVs equal the before-run except for the revision column | the choice is opt-in and changes nothing else |
| P2 | **Choices are real:** each of `gear`, `reserve` and `honed` is the best of the three for at least two of the 13 classes (class win rate in the rotation) | no card strictly dominates |
| P3 | **No trap card:** the rotation win rates of `gear`, `reserve` and `honed` are within 10 points of each other | every card is a sensible pick |
| P4 | **Bounded power:** the `mixed` rotation win rate is at most 8 points above the before-run's, and the slice 3/4 targets #188 met still hold (rotation 55–90%, every representative party at least 35%, finale highest defeat share, Dangerous at least 1.5x Guarded HP) | no power creep; do not undo slices 3–4 |
| P5 | **Specials reach the finale:** under `reserve`, specials per fight in fights 4–5 are at least 1.5x the before-run's | #187 item 3, for the player who chooses it |
| P6 | Gates zero (policy errors, illegal proposals, passive ends, protection mismatches), same seed same results, and no more stalls than the before-run | contract 07 |

**Levers**, smallest first: the Honed amounts and Reserve's +1, within the bounds of section 3. **Enemy numbers and catalog base numbers are not levers here.** If P3 or P4 cannot be met within the bounds, the report says missed and records a follow-up. #188's two misses (victories that lose a hero, and the melee party's gap) are reported, not targeted.

## 7. Tests (public seams; each red before green)

**Run and catalog** (`Project.Expedition.`):
- `Run.RewardCardsApplyOnce`: each card's effect, limit and potion supply on a revision 10 run; Gear at rank 3 gives potions only.
- `Run.RewardCardRejectionsLeaveStateUnchanged`: every rejection above, with full snapshot and RNG equality.
- `Run.GearOnlyRevision10MatchesRevision9`: identical fingerprints apart from the revision.
- `Run.OlderRevisionsAndSrdRewardGearOnly`.
- `Run.LevelGainsAreFactsOnly`: `FProgressionQuery` values; levels change no HP or pool; dead heroes keep their death level and upgrades.
- `Catalog.HonedFactsMatchForecastAndResolution`: all 70 kits, every honed special. Facts equal forecast equals the committed result, and the description is truthful.
- `Catalog.ReserveCapsEverywhere`: per-hero caps in facts, validation, camp and usage tags, for Kit, Kit + Slot, Rage, Wind and Wind + Maneuver builds.

**Persistence:**
- `Persistence.Schema5LoadsAsGearOnly`: **real schema 5 fixtures** written by unmodified staging after #214 (revision 8 and 9; route, mid-battle and reward phase) under `Tests/Fixtures/Schema5/`, with a README as for Schema4. Each resumes and continues exactly as staging's own continuation.
- `Persistence.UpgradeChoicesRoundTrip`: Reserve and Honed picks through write, read and Resume in every phase, continuing identically.
- `Persistence.RestoreRejectsUnearnedUpgrades`: flags without history; history without flags; two Reserve claims; a non-Gear claim on revision 9 or SRD; pools above `Caps`; a schema 6 document missing a field; a fractional version; version 7 refused, then preserved by New Run.
- `Persistence.FailedUpgradeClaimIsAtomic`: a failed write leaves the live state, the RNG and the file bytes unchanged.

**Presentation (PR B):**
- `Presentation.RewardCardsShowProgressionFacts`: card text equals the query, including every blocker.
- `PartyDetailsKeepClearOfPhasePanels`: run with the new reward panel size.
- An input-harness test: the keyboard picks a card and claims it. Hints and Help do not leak input, and Return to menu at the reward writes nothing.

**Balance:** `Project.Balance.RewardStrategiesSubmitOnlyLegalClaims`, and the `gear` strategy reproduces policy 1's rows.

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
- focus starts on Gear;
- the card focus is presentation state and is not saved. Resume at a reward shows Gear focused.

**Elsewhere:**
- the `+1 Lv` markers, hero details and the battle-log line (section 4);
- the action dock's usage tags and tooltips use the per-hero caps and honed amounts in the next fight (`Kit 3/3`, `Volley 5`);
- `RewardPanelRect` grows to fit the cards, and #200's placement rule must still hold.

**Captures** (`tools/capture/`, a `reward` pose set; gallery fixtures `reward-choice-*`):
- each card focused;
- Gear at rank 3;
- every card taken;
- Reserve on a Life cleric (Kit + Slot), a Barbarian (Rage) and a Battle Master (Wind + Maneuver);
- a fallen hero;
- each at 1280x720, 1920x1080 and 1600x600, inspected.

**On-screen pass** on a packaged build before PR B merges, with a real keyboard and pointer:
- claim each card;
- see the dock and tooltips change in the next fight;
- Resume at the reward and just after a claim;
- Return to menu at the reward;
- Help;
- the limits recorded honestly.

No new assets. A reward-claim sound cue belongs to slice 6.

## 9. Delivery and as built

- **PR A (rules, no `Presentation/`, `Content/` or `Config/`):** cards, catalogue, `Caps`, `FProgressionQuery`, revision 10, schema 6 with fixtures, harness policy 2, `14-progression-report.md`, and the rules reference, glossary (Reward card, Gear, Reserve, Honed, Progression facts) and current-state updates.
- **PR B (stacked on A):** section 8.
- **Gates for each PR:** build with 0 warnings, full `Project.` with 0 warnings, helpers on Python 3.11, `contribute.py check`, and two-axis review. PR A also needs the `Balance.Baseline.` before and after runs and an evidence branch. PR B also needs inspected captures and the on-screen pass before merge.
- **As built:** filled in by PR A (numbers, deviations, targets met or missed) and PR B (presentation), as in contracts 09 and 11.

## 10. Non-goals

- New specials, passives, a third special slot, or changes to any kit's base numbers in the shared catalog.
- HP growth, refills or new abilities on levelling; a level cap above 5; multiclassing; XP.
- Respec, undoing a claim, or picks for Downed or dead heroes; healing or revival from any card.
- Random or rerolled offers, or shops.
- Upgrades or unlocks that carry across runs. Run records (`FExpeditionEnding`) are unchanged.
- SRD-mode upgrades (#95 PR 5).
- Enemy stats, rosters or behaviour; #187's rescue loop, target choice and Monk items; #188's body-blocked stalemates.
- Audio cues (slice 6).
