# Slice 4 report: encounter identity and pacing, before and after

Child issue [#188](https://github.com/AustinKelsay/dnd-diceroll/issues/188), parent [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112). Contract: [11-encounter-identity-contract.md](11-encounter-identity-contract.md) (as built in its section 6). Previous report: [10-balance-tuning-report.md](10-balance-tuning-report.md).

## How it was measured

- Same harness and policy (version 1), 99 parties and 32 seeds as slices 3 and 3.2: 3,168 complete Compact runs per measurement ([07](07-balance-baseline-contract.md)).
- **Before:** this build, catalog revision 8 new runs (`DND_BALANCE_REVISION=8`). Rotation win rate 86.7%, defeat share by fight 0.1/0.1/0.3/5.2/7.7%: the #186 numbers.
- **After:** catalog revision 9 (the default). Each step was a full rebuild and a full `Balance.Baseline.` run under the machine lock at `--cores 2`; the kept step was rerun at the PR head and its CSVs compared byte for byte.
- Gates in every step: 0 policy errors, illegal proposals, passive ends and protection mismatches. Since the correction below the sweep also **fails on any deadlocked run** (a fight in which no unit can ever bring an opponent within range), and lists every other stall.
- **Cost.** *HP lost* is the slice 3 measure: party HP at the start of the fight minus at the end, as a share of the party's maximum HP at the start (net of healing). *HP taken* is the gross damage the enemies dealt, as a share of the same maximum. Parties reach fight 5 with 45–49% of their HP and the policy drinks potions when low, so net HP lost is bounded by arrival HP and offset by healing: it falls by stage even under revision 8, and a harder finale mostly loses more often. Pacing (E1) and the risk words (E3) are judged on HP taken and defeats; net HP lost is reported alongside and stated as missed where it misses.
- Read every number as "what this simple policy does", not as human difficulty.

## The correction: pocket-free layouts and a deadlock gate

PR #214's first head (step 4 below) had **2 stalls in 3,168 runs**, both real soft locks on the Dangerous sunken causeway. In `rot2-03` seed 20261119 (route pattern 10, fight 4) the last hero stood at (7, 8) with the causeway wall at (7, 9) below it and the bodies of the skirmisher, the raider and the archer at (7, 7), (6, 8) and (8, 8); the brute at (6, 6) could never reach a cell beside it and the hero could never move. Bodies occupy their cells and nothing ends a fight neither side can reach, so a player could only leave through Return to menu. Review also found two-body pockets in the first colonnade, (8, 5) between pillars (8, 4) and (8, 6), and the first sanctum, (c−4, 4) between the pillars in rows 3 and 5.

**Layout invariant (revision 9's layouts 3–6):** every open cell has at least three open orthogonal neighbours, except a board corner with no obstacle beside it. Equivalently, a cell's obstacle neighbours plus its off-board sides are at most one: no cell between two obstacles, no inside corner, no obstacle on the edge ring (so no one-wide corridor along the edge). A pocket that two bodies could close cannot exist. Three bodies beside one obstacle remain possible, exactly as along any board edge (every edge cell has three neighbours), which no layout can rule out; that rule risk is [#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217).

The four layouts were redrawn to it (contract section 6 lists the cells): pillars and stones two or more cells apart and off the edge ring; the causeway's walls at rows 2 and 8 with a three-cell central wall at row 5, which leaves four lanes each two cells wide, and puts terrain on the old trapped hero's cell (7, 8). `Run.EncounterIdentityLayoutsValidEverywhere` checks the invariant for every new layout at every size (16–18 by 11–12), route and stage, and that the traced configuration can no longer arise. `Project.Balance.DeadlockedStallIsRecognised` pins the traced configuration as a deadlock in the harness's classifier (and three variants as not), and every `Balance.Baseline.*` group asserts 0 deadlocks.

The approach layouts 0–2 are revision 7 and 8's unchanged recipes (the contract keeps fights 1–2 identical) and do not meet the invariant; in every measurement here (more than 30,000 approach fights) none stalled.

With the new layouts at step 4's numbers (step 8) one run stalled in the new colonnade: not a deadlock but #187's rescue loop (a bard revived a cleric that the 1-HP brute re-downed every round, while a step and an attack would have won). The pressure base then moved from 75% to 70% HP to restore the finale's lead in cost (step 10, kept), and that run no longer stalls.

## What changed

| | Approach (fights 1–2) | Pressure (fights 3–4) | Finale (fight 5) |
| --- | --- | --- | --- |
| Layouts | split stones, ruined pillars, broken wall (unchanged) | fallen colonnade, sunken causeway, cairn field | vault sanctum, front to back |
| Guarded roster | scout, archer | skirmisher, scout, archer | warden, scout, Vault bow |
| Dangerous roster | raider, raider, archer | brute, skirmisher, raider, archer | warden, brute, raider, Vault bow |
| Base | revision 8 | revision 8 with 70% HP, damage +3 | revision 8 with 90% HP, damage +3 |
| Risk word, Guarded / Dangerous | low / high | moderate / severe | high / severe |

Roles against the base for their fight: skirmisher 60% HP, armour +1, d4, speed 6; brute 110% HP, armour −1, d10, speed 3; warden 175% HP, armour +1, +2 damage. Raider and archer are the base. Fights 1–2 are revision 8's boards, seeds and enemies exactly (only the archer's name and the preview text differ).

## Steps tried

| Step | Change (cumulative) | Rotation win | Defeats fights 3 / 4 / 5 | Victories with a death (rotation) | HP taken, Dangerous: pressure / finale | Pressure layouts, rounds spread | Stalls | Kept |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| before | revision 8 | 86.7% | 0.3 / 5.2 / 7.7% | 16.0% | 57% / 45% | (recipes 0–2) 14% | 0 | baseline |
| 0 | contract starting numbers (skirmisher 60%, brute 150%, warden 200% HP, +1) | 50.9% | 4.4 / 16.8 / 34.8% | 11.7% | 74% / 55% | 20% | 0 | no: far below 55% |
| 1 | brute 120%, warden 150% | 55.4% | 3.7 / 14.2 / 31.8% | 10.5% | 71% / 52% | 18% | 2 | no: melee party 43.8% |
| 2 | base 80% HP and damage +3 in fights 3–5 | 79.2% | 1.7 / 5.4 / 13.9% | 10.5% | 58% / 50% | 14% | 0 | no: Dangerous finale costs less than pressure; layouts alike |
| 3 | brute 110%, warden 175% +2, causeway block (8, 3..7) | 79.3% | 1.3 / 5.6 / 14.3% | 11.1% | 55% / 51% | 28% | 0 | no: Dangerous finale still costs less |
| 4 | pressure base 75% HP, finale 90% | 77.2% | 0.9 / 5.0 / 17.5% | 11.8% | 53% / 56% | 29% | 2 (deadlocks) | first head of #214; superseded: soft locks |
| 5 | skirmisher die = base − 2 | 76.7% | 1.1 / 5.3 / 17.7% | 11.5% | 54% / 56% | 28% | 2 | no: no gain |
| 6 | step 4 with finale base 75% HP, damage +4 | 78.3% | 0.9 / 5.0 / 16.4% | 11.3% | 53% / 53% | 29% | 2 | no: finale no costlier than pressure |
| 7 | step 4 with pressure base 70% HP, damage +4 | 73.9% | 1.5 / 6.1 / 19.4% | 10.8% | 56% / 54% | 28% | 0 | no: worse on every open target |
| 8 | step 4 numbers on the pocket-free layouts | 75.2% | 1.6 / 6.1 / 17.9% | 11.6% | 56% / 55% | 15% | 1 (policy loop) | no: Dangerous finale costs less than pressure |
| **10** | **step 8 with pressure base 70% HP** | **78.1%** | **1.4 / 4.6 / 16.3%** | **11.3%** | **53% / 56%** | **15%** | **0** | **yes** |

(Step 9 was the same build as step 8 with the new deadlock classifier; it confirmed the step 8 stall was not a deadlock.)

The share of victories that lose a hero stayed at 10.5–11.8% in every revision 9 step: the bigger rosters turn the close finales that cost one hero under revision 8 into wipes or clean wins.

## Targets

| # | Target | Before (revision 8) | After (revision 9, step 10) | Verdict |
| --- | --- | --- | --- | --- |
| E1 | Pacing rises by stage for each route; defeat share by fight rises from fight 3 | HP taken Guarded 17 / 24 / 23%, Dangerous 46 / 57 / 45%; net HP lost falls by stage; defeats 0.3 / 5.2 / 7.7% | **HP taken Guarded 17 / 25 / 39%, Dangerous 46 / 53 / 56%; defeats 1.4 / 4.6 / 16.3%**; net HP lost Guarded 12 / 9 / 15%, Dangerous 27 / 25 / 27% | **met** on HP taken and defeats; missed on net HP lost (pressure below approach) |
| E2 | Within a stage, layouts differ by at least 15% in rounds or melee share | approach recipes: rounds 35% apart | **approach 35% (4.25 to 5.72 rounds), pressure 15% (5.37 to 6.18)**; melee share 8% and 6% | **met**, narrowly for pressure |
| E3 | A higher risk word costs more and loses more often, every pair | HP taken 17 / 24 / 38 / 53%, defeats 0.0 / 0.2 / 0.4 / 8.3% | **HP taken low 17%, moderate 25%, high 44%, severe 54%; defeats 0.0%, 0.6%, 2.3%, 11.9%**; net HP lost 12 / 9 / 23 / 26% | **met** on HP taken and defeats; missed on net HP lost for moderate vs low |
| E4 | Slice 3 targets hold | | | |
| | rotation win rate 55–90% | 86.7% | **78.1% (76.5–79.6%)** | met |
| | every representative party at least 35% | lowest 84.4% | **lowest 75.0% (support-poor)** | met |
| | at least 15% of victories lose a hero | rotation 16.0%, all parties 16.1% | **rotation 11.3%, all parties 11.1%** (first head: 11.8% and 11.7%) | **missed; accepted by the lead** (#187) |
| | Dangerous at least 1.5x Guarded HP (net, as slice 3) | 2.3–5.9x | **1.8x (fight 5) to 3.9x (fight 4)** | met |
| | the finale has the highest defeat share | 7.7% | **16.3%** | met |
| | melee within 20 points of ranged; melee damage gap at most 33% | melee 90.6%, Fighters 100%, ranged 87.5%; gap 18% | **melee 78.1%, Fighters 96.9%, ranged 81.2%; gap 15%** | met (the first head missed it by 1.9 points; that miss was accepted by the lead and is recorded in #187) |
| E5 | Gates zero, no stalls, same seed same results | met | **gates 0, 0 stalls, 0 deadlocks; kept step rerun at the head with byte-identical CSVs** | met |

## Stages by route

| Stage | Route | Defeats before → after | Rounds before → after | HP taken before → after | HP lost (net) before → after | Melee share before → after |
| --- | --- | --- | --- | --- | --- | --- |
| approach | guarded | 0.0% → 0.0% | 3.6 → 3.6 | 17% → 17% | 12% → 12% | 30% → 30% |
| pressure | guarded | 0.2% → 0.6% | 4.8 → 4.4 | 24% → 25% | 7% → 9% | 30% → 33% |
| finale | guarded | 0.9% → 6.8% | 5.0 → 7.2 | 23% → 39% | 5% → 15% | 32% → 39% |
| approach | dangerous | 0.2% → 0.2% | 6.4 → 6.4 | 46% → 46% | 27% → 27% | 36% → 36% |
| pressure | dangerous | 5.3% → 5.3% | 7.7 → 7.1 | 57% → 53% | 27% → 25% | 39% → 40% |
| finale | dangerous | 14.9% → 25.8% | 8.3 → 10.1 | 45% → 56% | 19% → 27% | 38% → 42% |

## Layouts (after, revision 9)

| Recipe | Layout | Route | Fights (stages) | Defeats | Rounds | HP taken | HP lost | Melee share |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | split stones | guarded | 594 (1) | 0.0% | 3.0 | 14% | 10% | 28% |
| 0 | split stones | dangerous | 1089 (1, 2) | 0.1% | 6.1 | 43% | 25% | 33% |
| 1 | ruined pillars | guarded | 1187 (1, 2) | 0.0% | 3.0 | 11% | 9% | 32% |
| 1 | ruined pillars | dangerous | 1088 (1, 2) | 0.0% | 5.6 | 41% | 25% | 37% |
| 2 | broken wall | guarded | 1386 (1, 2) | 0.0% | 4.4 | 23% | 15% | 30% |
| 2 | broken wall | dangerous | 990 (1, 2) | 0.4% | 7.5 | 54% | 32% | 38% |
| 3 | fallen colonnade | guarded | 375 (4) | 1.1% | 4.5 | 21% | 4% | 34% |
| 3 | fallen colonnade | dangerous | 495 (3) | 0.2% | 6.9 | 49% | 22% | 37% |
| 4 | sunken causeway | guarded | 884 (3, 4) | 0.6% | 4.7 | 26% | 7% | 32% |
| 4 | sunken causeway | dangerous | 1380 (3, 4) | 5.5% | 7.2 | 54% | 25% | 39% |
| 5 | cairn field | guarded | 1865 (3, 4) | 0.6% | 4.2 | 26% | 10% | 34% |
| 5 | cairn field | dangerous | 1283 (3, 4) | 7.1% | 7.0 | 54% | 26% | 42% |
| 6 | vault sanctum | guarded | 1492 (5) | 6.8% | 7.2 | 39% | 15% | 39% |
| 6 | vault sanctum | dangerous | 1483 (5) | 25.8% | 10.1 | 56% | 27% | 42% |

Before (revision 8, each recipe at every stage): split stones 4.1 / 6.8 rounds (Guarded / Dangerous), ruined pillars 3.6 / 6.6, broken wall 5.0 / 8.4; melee share 30–39%. The seeds draw recipes unevenly (the colonnade is fight 4 on Guarded and fight 3 on Dangerous in this seed set), so a layout's row mixes its stages; the stage table above is the comparison by stage.

What the layouts do: the **sunken causeway** is the longest fight of its stage, the **cairn field** the shortest and the bloodiest on the Dangerous road, the **colonnade** between. With two-wide lanes the causeway is less of a chokepoint than the first head's (pressure layouts now 15% apart in rounds, against 29%). Melee share does not separate them (36–38% pooled); the policy's heroes attack from where they stand.

## Parties (after)

| Party | Win rate before → after (95% CI after) | Rounds/fight before → after | Deaths/run before → after |
| --- | --- | --- | --- |
| `familiar` | 90.6% → 81.2% (65–91%) | 5.6 → 5.7 | 0.44 → 0.78 |
| `melee` | 90.6% → 78.1% (61–89%) | 6.6 → 6.7 | 0.56 → 0.94 |
| `ranged` | 87.5% → 81.2% (65–91%) | 5.4 → 5.2 | 0.59 → 0.88 |
| `support-poor` | 84.4% → 75.0% (58–87%) | 5.5 → 5.5 | 0.78 → 1.03 |
| `support-heavy` | 90.6% → 78.1% (61–89%) | 5.9 → 6.1 | 0.53 → 0.91 |
| `mixed` | 90.6% → 96.9% (84–99%) | 5.8 → 6.2 | 0.50 → 0.16 |
| `dup-fighter` | 100.0% → 96.9% (84–99%) | 6.5 → 6.6 | 0.25 → 0.28 |
| `dup-cleric` | 90.6% → 81.2% (65–91%) | 6.3 → 6.5 | 0.59 → 0.94 |
| `dup-wizard` | 87.5% → 87.5% (72–95%) | 5.3 → 5.3 | 0.69 → 0.53 |
| rotation (90 parties) | 86.7% → 78.1% (76.5–79.6%) | 5.8 → 6.0 | 0.69 → 0.97 |

By class (rotation), Fighter and Paladin 84% are highest and Monk 71% lowest (82.4% before), so the Monk follow-up from #184 still stands.

## Compatibility

- New runs are revision 9. Revision 7 and 8 runs keep their own generation, enemies, previews and archer name for life; restore regenerates each fight with the run's own revision (`OlderRevisionsResumeWithTheirOwnEnemies`: revision 8 and 9 checkpoints at a pressure route, mid-battle and between battles resume exactly and continue exactly as the uninterrupted run). The revision 8 seed-42 golden is unchanged and the revision 8 before-run reproduces #186's numbers, so the revision 8 path is untouched.
- Revision 9 draws the generation random numbers in revision 8's order: fights 1–2 keep their boards, seeds and capture fixtures (`CompactRevision9SeedFingerprint`).
- SRD keeps revision 7 generation at every revision. Unknown revisions are refused by name and preserved; schema 5 is unchanged.

## Follow-ups

1. **Bodies can seal a hero in on any layout** ([#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217)): the layouts now rule out pockets closed by two bodies, and the sweep fails on any deadlock, but three bodies beside an obstacle or the board edge remain possible until a rule change (bodies that do not block, or a stalemate rule).
2. **Losses without ending the run** (11.3% of victories lose a hero, target 15%; accepted by the lead): recorded in #187, whose rescue loop and target choice change how close fights end.
3. **Rescue loop** (#187 item 1): step 8's stall was this loop on the new colonnade.
4. **Melee share does not separate the pressure layouts**: a layout-aware damage split needs a policy that positions, not this one.
5. **Presentation** (PR #216): foe looks from the saved Role and obstacle kinds from `FExpeditionRun::Encounter`.
