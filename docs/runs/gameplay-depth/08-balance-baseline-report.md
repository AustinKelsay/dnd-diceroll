# Slice 3 baseline report: Compact balance before tuning

Child issue [#179](https://github.com/AustinKelsay/dnd-diceroll/issues/179), parent [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112) slice 3. Contract, policy and metric definitions: [07-balance-baseline-contract.md](07-balance-baseline-contract.md). **Nothing was tuned.** This is the measurement the tuning PR starts from.

## How it was produced

- Source: this branch's harness (test code only; no rules, RNG, save, catalog, `Config/` or `Content/` change) on staging `7f29981`, then again after merging staging `d73b8ba` (graphics #175 and #176), and again after the review fixes (the harness now reads protection and move costs from the game's own queries): every run, encounter and hero result is byte-identical across all three; only the blocker export changed format. Compact ruleset, catalog revision 7, policy version 1.
- Host: PlebDev Ultra, UE 5.8.4, `--cores 2`, under the machine lock.
- Command: `DND_BALANCE_OUT=.local/balance/<stamp> python3 tools/dev.py --cores 2 test --filter Balance.Baseline.`, then `python3 tools/balance_report.py <folder>`.
- Size: 99 parties x 32 seeds = **3,168 complete runs**, 15,840 fights, 169,887 hero turns. Harness time 20.4 s; locked wall time 45 s.
- Determinism: two separate engine processes produced byte-identical `runs.csv`, `encounters.csv` and `heroes.csv`; `Project.Balance.SameSeedSameResults` also checks it in-process, final snapshot included.
- Gates: 0 policy errors, 0 illegal proposals, 0 passive ends, 0 stalls, 0 protection mismatches (every enemy hit's committed prevention equals `FCombatBattle::Protection` before the hit).
- Data: the full `runs`, `encounters` and `heroes` CSVs of this baseline are on the PR's evidence branch, so every table here can be recomputed with `tools/balance_report.py`.

Read every number as "what this one simple policy does". Automated results do not prove human difficulty or fun.

## Headline

1. **The run is not threatening.** All 3,168 runs won all five fights (95% Wilson interval for the whole set: 99.9–100%). A fight takes 3.1 rounds on average and never more than 9. The party loses 2–9% of its HP per fight, and the closest call in any of the 15,840 fights left the party at 41% of its HP. 0.006 deaths per run.
2. **Win rate cannot rank parties or kits at this difficulty.** Every archetype, class and kit is at 100%. The usable signals are speed (rounds per fight), damage taken, the share of damage dealt, special availability and use, and resources left over.
3. **Under this greedy policy, specials disappear in the second half.** Specials used per fight by stage: **4.9, 3.7, 0.9, 0.4, 0.25**. 78% of Kit heroes end the run with an empty Kit, and every Barbarian ends with no Rage (2.98 Rages per run against a cap of 2 plus 1 at camp). The finale is fought almost entirely with basic attacks. The resource limits are fixed by the catalog; how early they run out depends on the policy (section "Policy artifacts versus likely balance problems").
4. **One special usually takes the whole shared pool.** In 46 of 70 kits, one of the two specials accounts for at least 80% of uses. The two specials share two Kit uses, so the second one is rarely seen.
5. **Melee does less of the work.** Melee classes (Fighter, Barbarian, Monk, Paladin) deal 7.0 damage per fight against 10.5 for the others. All-melee parties are the slowest (3.9–4.0 rounds per fight against 2.3–2.6 for ranged parties). The 5–7 cell opening gap costs melee heroes a turn of walking.
6. **Slot 1 takes the hits.** Enemies attack the first legal target in roster order, so slot 1 takes 2.8 HP per fight against 1.5–2.1 for the others, and 18 of the 19 deaths were slot 1.

## Parties and archetypes

| Party | Archetype | Builds | Win rate (95% CI) | Rounds/fight | Deaths/run | HP taken/run | Healing/run | Protection/run | Specials/run | Potions/run |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `familiar` | mixed | fighter.champion, rogue.thief, cleric.life, wizard.evocation | 100% (89%–100%) | 3.1 | 0.00 | 42 | 15 | 1 | 6.1 | 1.2 |
| `melee` | melee | barbarian.berserker, fighter.battle-master, paladin.devotion, monk.open-hand | 100% (89%–100%) | 3.9 | 0.00 | 58 | 18 | 3 | 12.1 | 2.0 |
| `ranged` | ranged | ranger.hunter, wizard.evocation, warlock.fiend, sorcerer.draconic-bloodline | 100% (89%–100%) | 2.6 | 0.00 | 36 | 13 | 1 | 11.6 | 1.8 |
| `support-poor` | support-poor | rogue.assassin, barbarian.beast, sorcerer.wild-magic, ranger.fey-wanderer | 100% (89%–100%) | 3.0 | 0.03 | 45 | 17 | 1 | 12.1 | 2.4 |
| `support-heavy` | support-heavy | paladin.devotion, cleric.life, druid.stars, bard.lore | 100% (89%–100%) | 3.1 | 0.00 | 41 | 14 | 1 | 9.0 | 1.3 |
| `mixed` | mixed | paladin.watchers, ranger.beast-master, cleric.peace, artificer.artillerist | 100% (89%–100%) | 3.2 | 0.00 | 35 | 9 | 4 | 9.3 | 0.9 |
| `dup-fighter` | duplicate | fighter.champion, fighter.champion, fighter.champion, fighter.champion | 100% (89%–100%) | 4.0 | 0.00 | 54 | 16 | 0 | 1.6 | 0.8 |
| `dup-cleric` | duplicate | cleric.life, cleric.life, cleric.life, cleric.life | 100% (89%–100%) | 3.3 | 0.03 | 48 | 22 | 4 | 9.3 | 0.6 |
| `dup-wizard` | duplicate | wizard.evocation, wizard.evocation, wizard.evocation, wizard.evocation | 100% (89%–100%) | 2.3 | 0.00 | 36 | 13 | 0 | 10.8 | 2.0 |
| rotation (90 parties) | rotation | all 70 kits, 5 or 6 parties each | 100% (100%–100%) | 3.1 | 0.01 | 42 | 15 | 2 | 10.1 | 1.9 |

"Damage dealt" is not in this table: every party won every fight, so it always equals the enemies' total HP (190 per run on average over the 32 route patterns).

## Classes (rotation parties)

| Class | Heroes | Win rate (95% CI) | Party rounds/fight | Death rate | Damage/fight | Kills/fight | Healing/fight | Protection/fight | Taken/fight |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ranger | 672 | 100% (99%–100%) | 2.92 | 0.0% | 11.3 | 0.76 | 0.54 | 0.06 | 1.55 |
| warlock | 832 | 100% (100%–100%) | 3.09 | 0.0% | 11.2 | 0.72 | 0.62 | 0.09 | 1.83 |
| wizard | 1600 | 100% (100%–100%) | 3.04 | 0.0% | 10.9 | 0.69 | 0.64 | 0.10 | 1.87 |
| rogue | 864 | 100% (100%–100%) | 3.02 | 0.6% | 10.6 | 0.70 | 0.95 | 0.12 | 2.63 |
| bard | 640 | 100% (99%–100%) | 3.05 | 0.0% | 10.5 | 0.69 | 0.92 | 0.04 | 2.43 |
| artificer | 640 | 100% (99%–100%) | 3.05 | 0.2% | 10.1 | 0.68 | 0.60 | 0.14 | 1.73 |
| sorcerer | 640 | 100% (99%–100%) | 3.08 | 0.2% | 10.0 | 0.64 | 0.88 | 0.19 | 1.93 |
| cleric | 1632 | 100% (100%–100%) | 3.08 | 0.4% | 9.8 | 0.63 | 0.89 | 0.14 | 2.07 |
| druid | 800 | 100% (100%–100%) | 3.02 | 0.1% | 9.7 | 0.64 | 0.74 | 0.11 | 1.92 |
| barbarian | 672 | 100% (99%–100%) | 3.11 | 0.1% | 7.7 | 0.55 | 0.53 | 0.14 | 2.46 |
| monk | 832 | 100% (100%–100%) | 3.16 | 0.2% | 7.0 | 0.48 | 1.05 | 0.19 | 2.94 |
| paladin | 832 | 100% (100%–100%) | 3.14 | 0.0% | 6.9 | 0.48 | 0.50 | 0.10 | 1.72 |
| fighter | 864 | 100% (100%–100%) | 3.09 | 0.0% | 6.6 | 0.43 | 0.60 | 0.16 | 2.07 |

Damage per fight is a **share of a fixed total** (every fight is won), so it says who gets there first and lands the blows, not raw power. Party rounds per fight is the average speed of the parties that include the class; it varies little (2.92–3.16), because three random partners dilute any one hero.

By slot (rotation):

| Slot | Death rate | Damage/fight | Taken/fight |
| --- | --- | --- | --- |
| 1 | 0.6% | 9.8 | 2.81 |
| 2 | 0.0% | 8.6 | 2.12 |
| 3 | 0.0% | 9.4 | 1.54 |
| 4 | 0.0% | 10.2 | 1.84 |

## Encounters by stage and route

| Fight | Route | Fights | Win rate | Rounds | HP lost | Lowest party HP, mean / worst | Downs per fight | Deaths per fight |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | dangerous | 1584 | 100% | 3.1 | 9% | 90% / 66% | 0.011 | 0.000 |
| 1 | guarded | 1584 | 100% | 2.2 | 3% | 97% / 80% | 0.000 | 0.000 |
| 2 | dangerous | 1584 | 100% | 3.5 | 9% | 89% / 70% | 0.001 | 0.001 |
| 2 | guarded | 1584 | 100% | 2.5 | 3% | 96% / 79% | 0.000 | 0.000 |
| 3 | dangerous | 1584 | 100% | 3.6 | 8% | 83% / 57% | 0.018 | 0.000 |
| 3 | guarded | 1584 | 100% | 3.3 | 5% | 87% / 62% | 0.000 | 0.000 |
| 4 | dangerous | 1584 | 100% | 3.8 | 9% | 75% / 46% | 0.040 | 0.000 |
| 4 | guarded | 1584 | 100% | 2.5 | 2% | 82% / 58% | 0.001 | 0.000 |
| 5 | dangerous | 1584 | 100% | 3.6 | 5% | 73% / 41% | 0.066 | 0.011 |
| 5 | guarded | 1584 | 100% | 2.6 | 2% | 77% / 55% | 0.000 | 0.000 |

- Dangerous fights (3 enemies) last 0.3 to 1.3 rounds longer and cost 1.7 to 3.7 times the HP of Guarded ones (2 enemies). Neither threatens the run.
- Pressure rises a little with stage (enemy HP grows by 2 per stage), but the party's level and gear rise faster. Downs concentrate in the Dangerous fights 4 and 5, still under 0.07 per fight.
- By recipe: the broken wall (recipe 2) gives the longest fights (4.3 rounds Dangerous, 3.2 Guarded) against 1.9–3.2 for the others; its one-cell gaps funnel the party. The split stones (0) and ruined pillars (1) play alike.
- Camp is taken after fight 1; potions accumulate faster than they are used (0.47 drunk per hero per run, 2.2 left at the end).

## Specials and resources

Each row's "available" is the share of the hero's turns on which `FActionFactsQuery` reported the special available at some point; the blocker is the most common first reason on the other turns, counting every first-step blocker of every run (the per-run counts are summed before the dominant one is chosen; `summary.json` keeps the full counts). The full per-kit table is in the appendix.

- **Kit exhaustion dominates.** For most kits the top blocker is `ResourceShort`: two Kit uses plus one at camp, three for five fights, and the policy spends them as soon as they help. Kits that end with Kit left are those whose specials are rarely worth using under this policy (exchanges, heals, protection when nobody is low or threatened): rogue.thief 1.7 of 2, paladin.glory 1.8, wizard.transmutation 1.6, cleric.peace 1.5.
- **Barbarians:** Rage is used 2.98 times per run; the second special needs Rage, so after the third Rage (usually fight 3) it is blocked by `StanceRequired` for the rest of the run.
- **Never used:** cleric.order's Ordered Exchange and wizard.order-of-scribes's Manifest Exchange (both Action-cost exchanges paired with a strong special that takes the pool).
- **Rarely available, not for lack of a resource** (top blocker `OutOfRange`): touch-range specials on ranged heroes, used only when an enemy walks up to the hero: cleric.war War Strike (6% of turns), cleric.tempest Thunder Shove (5%), druid.spores Spore Cloud (8%), rogue.assassin Ambush (10%), wizard.bladesinging Singing Blade (6%). The policy never closes to melee range to use them.
- **Available but rarely used** (51 flags in the summary). These are mostly the policy's narrow use of exchanges (only to pull a low ally out of melee), heals (only when someone is at half HP, which seldom happens) and protection (only when threatened and not already covered). Typical cases: Second Wind (49% available, 0.31 uses per run, Fighters end with 1.7 of 2), Life's Cure Wounds (0.42 per run), rogue.thief Quick Exchange (91% available, 0.18 uses).
- Leftover Slots: Life keeps 1.6 of 2 Slots and Evocation 1.4 of 2, because both share the Kit pool with a cheaper special.
- **Protection is small.** 0.0–0.5 HP prevented per fight per kit. Protection lasts only until the protector's next turn, and enemies deal 3.5 to 4.5 per hit on average.

## Stalls, deaths, long fights and outliers (with seeds)

Reproduce any of these with `DND_BALANCE_TRACE=<party>:<seed>:<route pattern> … test --filter Balance.Trace` (route pattern = seed − 20261109).

- **Stall, fixed in the policy before the baseline:** `familiar:20261116:23` (also `melee` and `rot3-08` on that seed). In the broken-wall map's one-cell gap a dead raider's body blocked the exit; the first policy measured distance through units, so nobody walked around through the other gaps, and the archer and the cleric's Rescue traded until round 60. The approach now measures distance with units blocking (contract rule 9), and `Project.Balance.PolicyNeverPassesALegalAction` runs that seed. Real-game note: bodies keep their cells for the whole fight, so a corpse can close a one-cell gap.
- **Deaths (19, all on won runs):** 10 rescue deadlines and 9 "still Downed when the fight was won". 17 are the same fight: seed `20261136` (route pattern 27), fight 5, the Dangerous vault on split stones. The slot-1 hero advances into range first and all three enemies, now at 20 HP, reach and hit it in their first turn. Two others: `rot3-07:20261131:22` fight 2 and `rot4-16:20261139:30` fight 5. Before the designated-rescuer rule (contract rule 3) there were 22. Example trace: `dup-cleric:20261136:27`, where the Downed cleric lies behind a wall and the nearest ally walks around it but arrives a turn after the third rescue turn.
- **Long fights:** the 99th percentile is 6 rounds; the longest are 9 rounds, all on the broken wall (recipe 2), for example `rot4-16:20261139:30` and `rot5-15:20261133:24`, both fight 4.
- **Outliers** (robust z of at least 3 across the 70 kits): ranger.hunter is the fastest kit (2.81 party rounds per fight); fighter.eldritch-knight (6.2) and paladin.glory (5.9) deal the least damage per fight; cleric.peace and monk.mercy heal the most; the self or Bonus protection kits (cleric.trickery, armorer, moon, arcane trickster, abjuration and others) prevent the most. The death-rate outliers (0.6–1.3%) are one or two deaths each from the seed-20261136 vault fight. No kit's win rate differs from the rotation's, since all are 100%.

## Policy artifacts versus likely balance problems

Likely **policy artifacts** (do not tune on these alone):
- The low use of exchanges, heals, Second Wind and protection: the policy uses them narrowly, and the fights rarely create the conditions (nobody is low).
- The touch-range specials of ranged heroes: a player could step in to use them.
- Rescue-failure deaths: the policy does not hold the last kill to save a Downed ally.
- One special taking the whole pool is partly the policy's greedy spending. But with three Kit uses for five fights even a careful player sees each special about once or twice per run.

Likely **real balance or design problems** (the policy cannot cause the limit itself, though it shapes when the limit is felt):
- Enemy pressure is far too low for a five-fight run with attrition: 100% wins, 2–9% HP lost per fight, closest call 41%. Enemies: 10–20 HP, AC 11–12, +2 to hit, d4+1 or d6+1, against heroes with 22–34 HP, AC 13–16, +5 to hit and d6+3 (casters d10).
- **Fixed resource limits** (catalog facts, independent of the policy): a Kit hero has at most three uses (two, plus one at camp) for five fights, shared by both specials, so at most 0.6 special uses per fight on average however they are spread. A Barbarian has at most three Rages, each lasting two of its own turns, so its Rage-gated special is usable on at most six turns per run (about 15 rounds).
- **Exhaustion timing** (policy-dependent): the greedy policy spends those uses as soon as they help, so they are gone by fight 3 and one special usually takes the whole pool. A player who saves uses would see specials later in the run, but no more of them, and the second special would still compete for the same three uses.
- The melee walking tax, and slot 1 taking most of the hits (the enemies' first-in-roster target choice).
- Bodies permanently blocking one-cell gaps on the broken-wall recipe.

## Candidate tuning targets for the next PR (not applied)

In priority order. Each needs before/after harness evidence and public regressions.

1. **Enemy pressure** (the precondition for every other measurement): scale enemy HP, to-hit or damage, or count, by stage and route, until the baseline policy loses some runs, Dangerous fights cost noticeably more than Guarded ones, and the finale is the hardest fight. This report sets no target: any win-rate band is an unvalidated design proposal that the tuning contract must state and justify (#112 warns against invented universal targets). All this baseline shows is that the current 100%, with a 41% closest call, is outside any range in which the harness can rank kits. Coordinate with slice 4 (encounter identity), which owns compositions; the numbers can move first.
2. **Special economy across five fights:** more Kit (and Rage) per run, per-fight recovery, or cheaper second specials, so both specials appear in later fights. Measure specials per fight by stage and the share of the second special.
3. **Barbarian Rage gating:** let the Rage-gated special work without Rage, or give more Rage, so the kit does not go blank after fight 3.
4. **Melee walking tax:** a smaller opening gap or more speed for melee roles. Weigh against slice 4's 5–7 cell gap decision.
5. **Touch-range specials on ranged classes** (War Strike, Thunder Shove, Spore Cloud, Singing Blade, Ambush): extend their range or accept them as deliberate close-range options.
6. **Enemy target choice:** nearest or weakest instead of first in roster order, so slot 1 is not the default tank. This is enemy behaviour, so it needs its own contract.
7. Lower priority, after 1 and 2: the low-impact kits (eldritch-knight, glory, champion) and how little protection prevents.

Re-run this baseline after each change: same seeds, same policy version, and a `Balance.Baseline.` diff.

## Limits

- One policy: simple, greedy, no look-ahead beyond one move, no retreat, no deliberate set-up (positioning for area, push or exchange). Kits whose value is set-up are under-measured.
- Compact only (SRD mode is out of scope).
- 32 seeds per party; route patterns are balanced, but board size and recipe follow the seed.
- Enemy behaviour is the game's own deterministic `PlayEnemyTurn`, unchanged.
- No human difficulty, readability or fun claim.

## Appendix: every kit (rotation, 160 or 192 hero-runs each)

| Kit | Party rounds/fight | Damage/fight | Healing/fight | Protection/fight | Special 1: available / uses per run / top blocker | Special 2: available / uses per run / top blocker | Left at victory (cap) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| artificer.alchemist | 2.89 | 10.1 | 0.67 | 0.00 | 24% / 0.14 / ResourceShort | 41% / 2.52 / ResourceShort | kit 0.2 (2), potions 2.3 |
| artificer.armorer | 3.13 | 9.3 | 0.66 | 0.43 | 22% / 2.89 / ResourceShort | 2% / 0.09 / ResourceShort | kit 0.0 (2), potions 2.2 |
| artificer.artillerist | 3.13 | 10.0 | 0.66 | 0.10 | 11% / 1.04 / ResourceShort | 16% / 1.94 / ResourceShort | kit 0.0 (2), potions 2.2 |
| artificer.battle-smith | 3.05 | 11.2 | 0.43 | 0.04 | 44% / 0.26 / OutOfRange | 13% / 1.68 / OutOfRange | kit 0.5 (2), potions 2.4 |
| barbarian.beast | 3.08 | 7.8 | 0.59 | 0.11 | 24% / 2.98 / ResourceShort | 21% / 1.61 / StanceRequired | rage 0.0 (2), potions 2.3 |
| barbarian.berserker | 3.12 | 8.1 | 0.48 | 0.15 | 23% / 2.99 / ResourceShort | 11% / 1.46 / StanceRequired | rage 0.0 (2), potions 2.3 |
| barbarian.totem-warrior | 3.17 | 6.7 | 0.53 | 0.21 | 24% / 2.98 / ResourceShort | 33% / 1.52 / StanceRequired | rage 0.0 (2), potions 2.3 |
| barbarian.wild-magic | 3.06 | 8.1 | 0.48 | 0.10 | 24% / 2.99 / ResourceShort | 24% / 2.19 / StanceRequired | rage 0.0 (2), potions 2.4 |
| bard.creation | 3.12 | 10.5 | 0.88 | 0.00 | 84% / 0.25 / ResourceShort | 20% / 1.07 / OutOfRange | kit 1.2 (2), potions 2.0 |
| bard.eloquence | 2.95 | 11.3 | 0.77 | 0.03 | 24% / 0.46 / ResourceShort | 19% / 2.50 / ResourceShort | kit 0.0 (2), potions 2.1 |
| bard.lore | 3.11 | 10.9 | 1.06 | 0.00 | 42% / 2.56 / ResourceShort | 21% / 0.13 / ResourceShort | kit 0.2 (2), potions 2.0 |
| bard.valor | 3.02 | 9.4 | 0.98 | 0.12 | 15% / 1.70 / ResourceShort | 11% / 1.24 / ResourceShort | kit 0.0 (2), potions 2.0 |
| cleric.knowledge | 3.05 | 10.3 | 0.99 | 0.00 | 29% / 0.03 / ResourceShort | 25% / 2.89 / ResourceShort | kit 0.0 (2), potions 1.9 |
| cleric.life | 3.01 | 9.2 | 1.11 | 0.16 | CureWounds 45% / 0.42 / OutOfRange | 77% / 1.95 / ResourceShort | kit 0.6 (2), slot 1.6 (2), potions 2.6 |
| cleric.light | 3.05 | 8.9 | 0.63 | 0.27 | 10% / 0.77 / ResourceShort | 21% / 2.17 / ResourceShort | kit 0.0 (2), potions 2.2 |
| cleric.nature | 3.07 | 9.8 | 0.81 | 0.00 | 58% / 1.36 / OutOfRange | 23% / 0.17 / OutOfRange | kit 0.9 (2), potions 2.3 |
| cleric.order | 3.10 | 9.5 | 0.91 | 0.32 | 21% / 0.00 / ResourceShort | 23% / 2.99 / ResourceShort | kit 0.0 (2), potions 2.0 |
| cleric.peace | 3.11 | 9.3 | 1.42 | 0.00 | 58% / 0.52 / NoWoundedTarget | 87% / 0.01 / OutOfRange | kit 1.5 (2), potions 2.3 |
| cleric.tempest | 3.16 | 11.6 | 0.66 | 0.00 | 5% / 0.71 / OutOfRange | 53% / 1.36 / ResourceShort | kit 0.5 (2), potions 2.2 |
| cleric.trickery | 3.03 | 9.6 | 0.65 | 0.50 | 24% / 2.94 / ResourceShort | 23% / 0.01 / ResourceShort | kit 0.0 (2), potions 2.2 |
| cleric.twilight | 3.12 | 10.5 | 0.78 | 0.10 | 56% / 0.86 / ResourceShort | 10% / 1.43 / OutOfRange | kit 0.2 (2), potions 2.0 |
| cleric.war | 3.08 | 9.9 | 0.92 | 0.04 | 6% / 0.80 / OutOfRange | 68% / 0.81 / ResourceShort | kit 0.7 (2), potions 2.1 |
| druid.land | 3.01 | 9.4 | 0.67 | 0.10 | 14% / 0.63 / ResourceShort | 20% / 2.29 / ResourceShort | kit 0.0 (2), potions 2.2 |
| druid.moon | 3.03 | 9.6 | 0.57 | 0.43 | 22% / 2.83 / ResourceShort | 4% / 0.16 / ResourceShort | kit 0.0 (2), potions 2.3 |
| druid.spores | 3.05 | 9.3 | 0.86 | 0.00 | 8% / 0.74 / OutOfRange | 32% / 0.28 / NoWoundedTarget | kit 1.1 (2), potions 2.4 |
| druid.stars | 3.00 | 10.8 | 0.90 | 0.00 | 40% / 2.72 / ResourceShort | 23% / 0.12 / ResourceShort | kit 0.1 (2), potions 2.2 |
| druid.wildfire | 2.98 | 9.3 | 0.70 | 0.00 | 45% / 2.21 / ResourceShort | 50% / 0.11 / ResourceShort | kit 0.2 (2), potions 2.3 |
| fighter.battle-master | 3.13 | 7.2 | 0.82 | 0.00 | SecondWind 48% / 0.31 / NoWoundedTarget | PrecisionStrike 15% / 1.99 / ResourceShort | maneuver 0.0 (2), wind 1.7 (2), potions 2.4 |
| fighter.champion | 3.14 | 6.5 | 0.70 | 0.00 | SecondWind 49% / 0.31 / NoWoundedTarget | - | wind 1.7 (2), potions 2.4 |
| fighter.eldritch-knight | 3.06 | 6.2 | 0.56 | 0.36 | 22% / 2.37 / ResourceShort | 9% / 0.56 / ResourceShort | kit 0.0 (2), potions 2.3 |
| fighter.psi-warrior | 3.08 | 6.7 | 0.50 | 0.17 | 19% / 2.24 / ResourceShort | 8% / 0.73 / ResourceShort | kit 0.0 (2), potions 2.4 |
| fighter.rune-knight | 3.03 | 6.7 | 0.41 | 0.23 | 30% / 2.06 / ResourceShort | 11% / 0.89 / ResourceShort | kit 0.0 (2), potions 2.4 |
| monk.astral-self | 3.17 | 6.6 | 0.90 | 0.41 | 5% / 0.12 / ResourceShort | 22% / 2.84 / ResourceShort | kit 0.0 (2), potions 2.1 |
| monk.four-elements | 3.21 | 8.0 | 1.10 | 0.00 | 26% / 2.81 / ResourceShort | 20% / 0.03 / ResourceShort | kit 0.0 (2), potions 1.9 |
| monk.mercy | 3.21 | 7.0 | 1.32 | 0.00 | 11% / 0.08 / OutOfRange | 33% / 2.40 / OutOfRange | kit 0.3 (2), potions 1.8 |
| monk.open-hand | 3.15 | 6.9 | 1.18 | 0.33 | 10% / 1.32 / ResourceShort | 24% / 1.62 / ResourceShort | kit 0.0 (2), potions 1.8 |
| monk.shadow | 3.09 | 6.7 | 0.78 | 0.22 | 67% / 0.12 / ResourceShort | 57% / 1.81 / ResourceShort | kit 0.4 (2), potions 2.2 |
| paladin.ancients | 3.17 | 6.6 | 0.42 | 0.07 | 8% / 0.66 / ResourceShort | 19% / 2.27 / ResourceShort | kit 0.0 (2), potions 2.4 |
| paladin.devotion | 3.09 | 6.6 | 0.51 | 0.31 | 84% / 1.41 / OutOfRange | 20% / 0.12 / OutOfRange | kit 1.0 (2), potions 2.5 |
| paladin.glory | 3.21 | 5.9 | 0.56 | 0.00 | 86% / 0.14 / OutOfRange | 23% / 0.13 / OutOfRange | kit 1.8 (2), potions 2.5 |
| paladin.vengeance | 3.01 | 8.8 | 0.49 | 0.00 | 20% / 2.66 / ResourceShort | 36% / 0.03 / ResourceShort | kit 0.0 (2), potions 2.4 |
| paladin.watchers | 3.19 | 6.8 | 0.50 | 0.15 | 5% / 0.23 / ResourceShort | 22% / 2.73 / ResourceShort | kit 0.0 (2), potions 2.4 |
| ranger.beast-master | 2.94 | 10.7 | 0.45 | 0.27 | 6% / 0.05 / ResourceShort | 24% / 2.86 / ResourceShort | kit 0.0 (2), potions 2.4 |
| ranger.fey-wanderer | 3.02 | 11.6 | 0.59 | 0.00 | 28% / 0.02 / ResourceShort | 25% / 2.90 / ResourceShort | kit 0.0 (2), potions 2.3 |
| ranger.hunter | 2.81 | 12.1 | 0.40 | 0.00 | 22% / 0.24 / ResourceShort | 22% / 2.71 / ResourceShort | kit 0.0 (2), potions 2.4 |
| ranger.swarmkeeper | 2.87 | 10.9 | 0.70 | 0.00 | 9% / 1.19 / ResourceShort | 35% / 1.66 / ResourceShort | kit 0.1 (2), potions 2.2 |
| rogue.arcane-trickster | 2.90 | 9.4 | 0.74 | 0.43 | 24% / 2.83 / ResourceShort | 11% / 0.12 / ResourceShort | kit 0.0 (2), potions 2.2 |
| rogue.assassin | 3.06 | 10.6 | 1.09 | 0.00 | 10% / 1.07 / OutOfRange | 53% / 0.02 / OutOfRange | kit 1.2 (2), potions 1.9 |
| rogue.phantom | 3.06 | 10.6 | 0.85 | 0.12 | 12% / 0.19 / ResourceShort | 22% / 2.78 / ResourceShort | kit 0.0 (2), potions 2.1 |
| rogue.soulknife | 3.03 | 11.9 | 0.90 | 0.00 | 21% / 2.90 / ResourceShort | 25% / 0.03 / ResourceShort | kit 0.0 (2), potions 2.1 |
| rogue.thief | 3.06 | 10.5 | 1.21 | 0.00 | 91% / 0.18 / OutOfRange | 25% / 0.14 / OutOfRange | kit 1.7 (2), potions 1.9 |
| sorcerer.aberrant-mind | 3.09 | 10.1 | 0.73 | 0.17 | 10% / 1.01 / ResourceShort | 16% / 1.99 / ResourceShort | kit 0.0 (2), potions 2.1 |
| sorcerer.clockwork-soul | 3.15 | 9.6 | 1.16 | 0.31 | 80% / 1.96 / ResourceShort | 52% / 0.42 / NoWoundedTarget | kit 0.6 (2), potions 2.3 |
| sorcerer.draconic-bloodline | 3.07 | 10.2 | 0.66 | 0.29 | 4% / 0.19 / ResourceShort | 22% / 2.77 / ResourceShort | kit 0.0 (2), potions 2.2 |
| sorcerer.wild-magic | 3.00 | 10.2 | 0.99 | 0.00 | 25% / 2.87 / ResourceShort | 29% / 0.04 / ResourceShort | kit 0.0 (2), potions 1.9 |
| warlock.archfey | 2.98 | 11.2 | 0.44 | 0.00 | 15% / 1.88 / OutOfRange | 64% / 0.13 / ResourceShort | kit 0.4 (2), potions 2.4 |
| warlock.fathomless | 3.09 | 10.3 | 0.61 | 0.30 | 4% / 0.20 / ResourceShort | 21% / 2.79 / ResourceShort | kit 0.0 (2), potions 2.2 |
| warlock.fiend | 3.10 | 11.6 | 0.71 | 0.00 | 32% / 2.88 / ResourceShort | 6% / 0.04 / ResourceShort | kit 0.0 (2), potions 2.2 |
| warlock.genie | 3.15 | 10.8 | 0.65 | 0.11 | 75% / 0.16 / ResourceShort | 83% / 1.22 / ResourceShort | kit 0.9 (2), potions 2.2 |
| warlock.great-old-one | 3.06 | 12.1 | 0.69 | 0.04 | 22% / 0.23 / ResourceShort | 20% / 2.73 / ResourceShort | kit 0.0 (2), potions 2.2 |
| wizard.abjuration | 3.11 | 10.4 | 0.62 | 0.42 | 23% / 2.96 / ResourceShort | 3% / 0.03 / ResourceShort | kit 0.0 (2), potions 2.2 |
| wizard.bladesinging | 2.99 | 10.3 | 0.63 | 0.08 | 6% / 0.79 / OutOfRange | 73% / 1.23 / ResourceShort | kit 0.4 (2), potions 2.2 |
| wizard.conjuration | 2.97 | 10.3 | 0.56 | 0.00 | 80% / 0.21 / ResourceShort | 21% / 1.21 / OutOfRange | kit 1.0 (2), potions 2.3 |
| wizard.divination | 3.16 | 11.4 | 0.59 | 0.20 | 11% / 0.94 / ResourceShort | 15% / 2.06 / ResourceShort | kit 0.0 (2), potions 2.3 |
| wizard.enchantment | 3.01 | 10.4 | 0.75 | 0.00 | 10% / 1.32 / OutOfRange | 74% / 0.16 / ResourceShort | kit 0.8 (2), potions 2.2 |
| wizard.evocation | 3.03 | 12.4 | 0.70 | 0.00 | MagicMissile 17% / 1.54 / ResourceShort | 11% / 1.46 / ResourceShort | kit 0.0 (2), slot 1.4 (2), potions 2.2 |
| wizard.illusion | 3.10 | 10.4 | 0.45 | 0.30 | 23% / 2.99 / ResourceShort | 21% / 0.01 / ResourceShort | kit 0.0 (2), potions 2.4 |
| wizard.necromancy | 2.91 | 11.0 | 0.73 | 0.00 | 36% / 2.86 / ResourceShort | 12% / 0.05 / ResourceShort | kit 0.1 (2), potions 2.1 |
| wizard.order-of-scribes | 2.94 | 11.5 | 0.61 | 0.00 | 23% / 2.97 / ResourceShort | 23% / 0.00 / ResourceShort | kit 0.0 (2), potions 2.3 |
| wizard.transmutation | 3.13 | 10.6 | 0.78 | 0.00 | 88% / 0.23 / OutOfRange | 25% / 0.15 / OutOfRange | kit 1.6 (2), potions 2.3 |
