# Slice 3 child contract: seeded balance harness and baseline

Status: **child issue [#179](https://github.com/AustinKelsay/dnd-diceroll/issues/179); written 2026-11-09 under [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112)'s delegated authority** (slice 3, "Party balance and subclass usefulness"). Fixed review baseline: staging `7f2998125b8a75e1b7c0794ea0ed331ae567872e` (#173 merged). This contract covers **the harness and the baseline report only**. Tuning is a later PR that must cite this baseline and rerun it.

## 1. Purpose and limits

A reproducible, headless, seeded harness plays complete Compact expeditions (route, five fights, rewards, camp, ending) with a fixed, documented party policy, and reports what happened. The baseline it produces is the evidence the tuning PR starts from.

Automated policy results are **not** proof of human difficulty or fun. They measure what one simple, consistent player does with each kit. A kit that looks weak may be weak, or may be something this policy plays badly. The report separates the two where it can (section 8).

Out of scope: SRD mode (#95), any rules, RNG, save, catalog, `Config/` or `Content/` change, enemy behaviour changes, tuning, presentation, capture, camera and world code, and human playtesting.

## 2. Public seams the harness uses

- `FExpeditionRun` only: `NewRun(Seed, Party, Error, Identity, ERuleset::Compact)`, `ChooseRoute`, `Submit`, `PlayEnemyTurn`, `ClaimReward`, `Recover`, and the const queries `Progress()`, `Battle()`, `Snapshot()`.
- Combat queries through `Run.Battle()`: `ActiveId`, `Find`, `LegalMoves`, `LegalTargets`, `PathTo` (the movement cost of every legal destination the policy compares), `Forecast`, `Snapshot`, and `Protection` (added for review: the const public form of the reduction resolution applies, with its source; no behavior change). The policy never re-derives a rule: its only own computations are the approach-distance heuristics of rules 3 and 9.
- `FActionFactsQuery::For` / `BarFor` (#58): which kit specials are available on each turn, and the exact blocker when not.
- `FBuildCatalog::All` / `Find` for the party set and resource caps.

The harness never calls a private or `friend` API, never edits a snapshot, and never restores one into a run. The one look-ahead it does (section 3, "look-ahead") submits a `Move` on a **copy** of the run and discards the copy. A Compact move consumes no random numbers, so the real run's RNG is untouched.

**No checkpoint.** `FExpeditionRun` has no persistence; only `FExpeditionSession` writes files. The harness never constructs a session, so it cannot write the player's checkpoint or any save. A test asserts that the default checkpoint path is byte-identical (or still absent) after a harness run.

**Determinism.** Every choice is a pure function of the public state, with fixed tie-breaks (lowest HP, then lowest ID, then cell order Y then X). No time, hash-map iteration order or pointer value enters a decision. Run identity is fixed (`balance-<party>-<seed>`), so the same party and seed give byte-identical result rows and final snapshot, which a test checks.

## 3. Party policy (deterministic, legal commands only)

On each party turn the policy repeats one decision step until it chooses End Turn, the battle ends, or 16 steps pass (a stall, section 7). Each step takes the **first** rule below that applies. Every command it submits is first checked against the authority: a target must be in `LegalTargets(actor, command)` and a destination in `LegalMoves(actor)`. A proposal outside those lists is never submitted: it counts as an illegal proposal and the hero ends its turn instead. A submitted command the run rejects counts as a policy error. Both gates are zero.

"Threatened" means some conscious enemy is within its own `Speed + Range` cells (Manhattan) of that hero, that is, it can attack this hero on its next turn. "Low" means conscious with HP at or below half of max HP. Expected damage is the forecast's average `(Min + Max) / 2` times hit chance; an area special adds every listed victim's loss.

1. **Rage.** Rage is legal and an enemy is within `Speed + Range + 1` of the Barbarian: Rage (Bonus Action). Rage-gated specials come later.
2. **Revive.** A Downed ally is adjacent: Potion on them if legal (heals 2d4+2), otherwise Rescue (1 HP). Uses the Action.
3. **Reach a Downed ally.** The Action is unspent and a Downed ally is not adjacent: move to the nearest legal cell (fewest steps, then cell order) from which a look-ahead shows Rescue or Potion legal on them. If none is reachable this turn, the **designated rescuer** (the conscious ally with the shortest walking distance to a free cell beside the most urgent Downed hero, fewest rescue turns left first; units block; ties to the lowest ID) steps to the legal cell that brings it closest, and may still attack afterwards.
4. **Heal.** A healing command (Second Wind, Cure Wounds, a kit Healing special, Potion) is legal on a low hero: heal the hero with the lowest HP fraction (then lowest ID), with Bonus Action heals first, then the larger expected heal, Potion last on ties.
5. **Bonus protection.** A Bonus Action Protect special is legal on a threatened hero who is not already protected by at least that amount (per `FCombatBattle::Protection`): protect the one with the lowest HP fraction.
6. **Exchange.** A Swap special is legal on a low ally adjacent to an enemy, and the actor is not low and has at least 25 points more HP fraction: swap with the lowest such ally.
7. **Damage special.** For each legal damaging special (Strike, Surge, Push, Area, Magic Missile, Precision Strike) take its best target: a guaranteed drop (`Min >= HP`) first, then the highest expected damage, then lowest HP, then lowest ID.
   - A Bonus Action special is used whenever it has a target.
   - An Action special is used when the basic Attack is not legal, or the special guarantees a drop, or its expected damage beats the best basic Attack's.
   - **Healer's reserve:** a kit with a Kit-paid Healing special does not spend its last Kit use on anything else (damage, protection, exchange) unless that use guarantees a drop.
8. **Attack** the legal target with the lowest HP (then lowest ID).
9. **Move.** The Action is unspent and no Attack is legal:
   - to a legal cell from which a look-ahead shows Attack legal. Candidates are prefiltered by Manhattan range; ranged heroes (Range 2 or more) prefer the cell farthest from the nearest enemy, melee heroes the fewest steps; then cell order. At most 16 look-aheads;
   - otherwise toward the enemy: the legal cell with the smallest walking distance to any conscious enemy, only if it is closer than where the hero stands. The distance is a breadth-first search from the snapshot in which other units (bodies included) block; only when that finds no closer cell does it fall back to terrain alone. (A terrain-only first version jammed the party in a one-cell wall gap behind a body until round 60; see the report.)
10. **Action protection.** An Action Protect special is legal on a threatened hero: as rule 5. Reached only when no attack was possible this turn.
11. **End Turn.**

Between fights: the **reward** goes to the conscious survivor with the lowest gear rank (then lowest ID), spreading gear; the **camp** recovery is always taken; the **route** follows the run's route pattern (section 4).

Why this shape: it is the brief's suggested ladder (drop, heal, attack lowest, approach, end) with the gaps a full kit list exposes filled by one stated rule each: revive before fighting (an unrescued Downed hero dies at the deadline or when the battle is won), protection and exchange only when they prevent damage, and a small healer's reserve. It deliberately does no search, no kiting after acting and no route evaluation. Those limits are recorded as policy artifacts, not balance facts.

Known limits (listed again in the report): a hero can finish the last enemy while an ally is Downed elsewhere, which kills that ally at victory; heroes never retreat; Swap is valued only as a rescue from melee; a special whose best use is set-up (Push into a better position) is valued only for its damage.

## 4. Party set and seeds

**Seeds.** Each party plays seeds `20261109 + i` for `i = 0 … 31`. The route pattern is the bits of `i`: bit `s` set means the Dangerous road at stage `s`. So every party plays all 32 route patterns exactly once, and encounters 1 to 5 each see 16 Guarded and 16 Dangerous fights per party (fewer once runs end early).

**Representative parties** (slot 1 first; enemies attack the first legal target in roster order, so slot 1 takes the most hits):

| Party | Archetype | Builds |
| --- | --- | --- |
| `familiar` | mixed (default preset) | fighter.champion, rogue.thief, cleric.life, wizard.evocation |
| `melee` | melee | barbarian.berserker, fighter.battle-master, paladin.devotion, monk.open-hand |
| `ranged` | ranged | ranger.hunter, wizard.evocation, warlock.fiend, sorcerer.draconic-bloodline |
| `support-poor` | support-poor (no heals, no ally protection) | rogue.assassin, barbarian.beast, sorcerer.wild-magic, ranger.fey-wanderer |
| `support-heavy` | support-heavy | paladin.devotion, cleric.life, druid.stars, bard.lore |
| `mixed` | mixed | paladin.watchers, ranger.beast-master, cleric.peace, artificer.artillerist |
| `dup-fighter` | duplicate | 4 x fighter.champion |
| `dup-cleric` | duplicate | 4 x cleric.life |
| `dup-wizard` | duplicate | 4 x wizard.evocation |

**Coverage rotation.** Five passes over the 70 kits in `FBuildCatalog::All` order. Pass `p` uses multiplier `m = {11, 13, 17, 19, 23}[p]` (each coprime to 70) and lists kit `(j * m + 3p) mod 70` at position `j`; party `r` of the pass takes positions `4r … 4r+3` (mod 70), `r = 0 … 17`. That is 90 parties; every kit appears in 5 or 6 parties, in varied slots and with varied partners, and all 13 classes appear in every pass. A test checks the coverage.

Total: 99 parties x 32 seeds = 3,168 complete runs.

## 5. Metrics and definitions

Per **run**: outcome (Victory, Defeat, Stall), encounters won, rounds (the battle's round counter at each fight's end, summed), deaths, survivors, party damage dealt, damage taken, healing, protection, specials used, potions used, commands submitted.

Per **encounter**: stage 1 to 5, route (Guarded, Dangerous), recipe, board size, enemy count, outcome, rounds, party HP at start and end (and max), damage dealt and taken, healing, protection, downs, deaths, rescues.

Per **hero per run**: build, class, slot, survived, the stage and cause of death (rescue deadline, Downed at victory, wipe), final level, turns taken, idle turns (only End Turn while conscious), damage dealt, kills, healing done, protection given, damage taken, downs, revives done, potions used, and per kit special: turns on which it was available, uses, and the most common first blocker on turns it was not. Resources held at the end: Kit, Slot, Wind, Maneuver, Rage, Potions (with caps).

Definitions:
- **Damage dealt** is enemy HP removed by the hero's own command (snapshot difference, so protection and the zero floor are already applied). **Kills** are enemies whose life became Dead during it.
- **Healing** is party HP restored by the hero's command (a Rescue counts its 1 HP; overheal does not exist because healing is capped by missing HP).
- **Protection** is `DamagePrevented` on enemy attacks, credited to the `SourceId` of `FCombatBattle::Protection` taken as the enemy turn began: the guard with the largest amount (then roster order), otherwise the target's own Rage. Every hit is also checked against that query: a committed `DamagePrevented` other than `min(raw damage, Amount)` is a protection mismatch (gate zero).
- **Available** means `FActionFactsQuery::For(...).bAvailable` at any decision step of that turn. The blocker recorded for an unavailable turn is the first blocker at the turn's first step. Every blocker's turn count is exported per hero per run (`ResourceShort:12;OutOfRange:3`), and the report sums them before choosing a dominant blocker.

## 6. Where results go and how to run it

Harness code lives with the tests (`Source/DnDDiceRoll/Tests/Balance*`), so no shipping code changes.

- `Project.Balance.*`: small policy, coverage and determinism tests, part of the ordinary `Project.` gate (a few seconds).
- `Balance.Baseline.<Group>`: the bulk baseline, deliberately **outside** the `Project.` prefix so ordinary gates never run it. Groups: `Fixed` (the nine representative parties) and `Rotation1` … `Rotation5` (one pass each). The filter `Balance.Baseline.` runs all six in one process. Each group is a latent command that plays one party (32 runs) per frame: a single 20-second block on the game thread made the engine's own pending HTTP connectivity check time out and log warnings.
- `Balance.Trace`: writes one run as a text log (board each round, every command and result) for reproducing a stall, death or outlier: `DND_BALANCE_TRACE=<party>:<seed>:<route pattern>`, for example `familiar:20261116:23`, into `<out>/trace/`.

```sh
export DND_BALANCE_OUT=.local/balance/$(date -u +%Y%m%dT%H%M%SZ)   # optional; one folder for all groups
flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock python3 tools/dev.py --cores 2 test --filter Balance.Baseline.
python3 tools/balance_report.py "$DND_BALANCE_OUT"
```

Each group writes `<out>/<group>/runs.csv`, `encounters.csv`, `heroes.csv` and `group.json` (party list, seeds, policy version, run count, elapsed time). Without `DND_BALANCE_OUT` the folder is `.local/balance/<UTC stamp>/`. **Both the harness and `tools/balance_report.py` refuse any folder outside the checkout's `.local/`** (relative paths are taken from the checkout root and `..` is collapsed first; a symlink inside `.local/` is not followed). `tools/balance_report.py` (standard library only) merges every group under the folder into `summary.json` and `summary.md`. `.local/` is ignored; the committed baseline report quotes the summary.

**Runtime budget:** the whole baseline in at most 240 s of test time at `--cores 2` (the helper's test process limit is 300 s), and well under ten minutes of locked wall time. If it does not fit, groups run as separate locked jobs into the same `DND_BALANCE_OUT`. The `Project.Balance.` tests stay under 10 s together. **Measured:** 3,168 runs in 20.4 s of harness time, 45 s of locked wall time including editor start and exit; `Project.Balance.` about 0.4 s.

## 7. Stalls, anomalies and outliers

- **Stall:** a fight reaching round 60 without an outcome, or a party turn reaching 16 steps. The run stops and is reported as `Stall` with its party and seed. Target: zero.
- **Policy error:** a command the run rejected, or a proposal that was not in the legal lists. Gate: zero.
- **Protection mismatch:** see section 5. Gate: zero.
- **Passive end:** the policy ended a turn with its Action unspent while an Attack was legal, or while it had movement left and a legal move closer to an enemy by rule 9's distance (the same function decides both). Gate: zero (this is the "never stalls while a legal action exists" check).
- **Idle turns** (only End Turn) are reported per kit, not gated.
- **Outlier kit:** on a per-kit metric (win rate of the parties containing it, damage per fight present, healing, protection, death rate), a robust z-score `|x - median| / (1.4826 x MAD)` of at least 3 across the 70 kits, or a win rate at least 15 points from the rotation mean whose 95% Wilson interval excludes it.
- **Unusable signature:** a kit special available on under 10% of the hero's turns for a reason other than an empty resource, or never used in any run. "Rarely used when available" (under 10% of available turns) points at the policy rather than the kit.
- **Long fight:** an encounter at or above the 99th percentile of rounds, listed with seed.

## 8. Tests (`Project.Balance.*`)

1. `PolicySubmitsOnlyLegalCommands`: six diverse parties x two seeds; zero policy errors, every proposal in the legal lists, every submit accepted.
2. `PolicyNeverPassesALegalAction`: the same runs; zero passive ends and zero stalls; every run ends in Victory or Defeat.
3. `SameSeedSameResults`: two parties x two seeds, played twice: identical result rows and identical final `FExpeditionSnapshot`; two different seeds differ.
4. `HarnessWritesNoCheckpoint`: the default checkpoint path is unchanged after harness runs.
5. `PartySetCoversEveryKit`: every party uses valid catalog builds; the rotation covers all 70 kits at least five times and all 13 classes in each pass; the representative archetypes are present.
6. `OutputStaysUnderLocal`: the results root is accepted inside `.local/` and refused for `../`, absolute paths elsewhere, `.local/..`, `.local` itself and look-alike prefixes.

Also `Project.Expedition.Combat.ProtectionQueryMatchesResolution`: the new query equals the committed prevention and names its source for two wards, Rage and a Rage-gated Shelter, and changes no state.

Helper tests cover `tools/balance_report.py` (merge, rates, Wilson interval, robust z, outlier and signature flags, refusal of an empty folder, blocker counts summed before ranking, refusal of folders outside `.local/`).

## 9. Delivery

One PR to `staging`: this contract, the harness and its tests, the report tool and its tests, and [08-balance-baseline-report.md](08-balance-baseline-report.md) with the baseline. No tuning. Evidence: doctor, helper tests, `contribute.py check`, build with zero warnings, full `Project.` with zero warnings, one explicit `Balance.Baseline.` run, and selected summaries on an `evidence/pr<number>-<sha>` branch.
