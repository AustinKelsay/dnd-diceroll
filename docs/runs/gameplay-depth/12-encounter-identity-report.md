# Slice 4 report: encounter identity and pacing, before and after

Child issue [#188](https://github.com/AustinKelsay/dnd-diceroll/issues/188), parent [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112). Contract: [11-encounter-identity-contract.md](11-encounter-identity-contract.md) (as built in its section 6). Previous report: [10-balance-tuning-report.md](10-balance-tuning-report.md).

## How it was measured

- Same harness and policy (version 1), 99 parties and 32 seeds as slices 3 and 3.2: 3,168 complete Compact runs per measurement ([07](07-balance-baseline-contract.md)).
- **Before:** this build, catalog revision 8 new runs (`DND_BALANCE_REVISION=8`). Rotation win rate 86.7%, victories with a death 16.0%, defeat share by fight 0.1/0.1/0.3/5.2/7.7%: the #186 numbers.
- **After:** catalog revision 9 (the default). Each step was a full rebuild and a full `Balance.Baseline.` run under the machine lock at `--cores 2`; the kept step was rerun at the PR head and its CSVs compared.
- Gates in every step: 0 policy errors, illegal proposals, passive ends and protection mismatches. Stalls: 0 before; 2 of 3,168 runs in the kept step (below).
- **Cost.** *HP lost* is the slice 3 measure: party HP at the start of the fight minus at the end, as a share of the party's maximum HP at the start (net of healing). *HP taken* is the gross damage the enemies dealt, as a share of the same maximum. Parties reach fight 5 with 45–49% of their HP and the policy drinks potions when low, so net HP lost is bounded by arrival HP and offset by healing: it falls by stage even under revision 8, and a harder finale mostly loses more often. Pacing (E1) and the risk words (E3) are judged on HP taken and defeats; net HP lost is reported alongside and stated as missed where it misses.
- Read every number as "what this simple policy does", not as human difficulty.

## What changed

| | Approach (fights 1–2) | Pressure (fights 3–4) | Finale (fight 5) |
| --- | --- | --- | --- |
| Layouts | split stones, ruined pillars, broken wall (unchanged) | fallen colonnade, sunken causeway, cairn field | vault sanctum, front to back |
| Guarded roster | scout, archer | skirmisher, scout, archer | warden, scout, Vault bow |
| Dangerous roster | raider, raider, archer | brute, skirmisher, raider, archer | warden, brute, raider, Vault bow |
| Base | revision 8 | revision 8 with 75% HP, damage +3 | revision 8 with 90% HP, damage +3 |
| Risk word, Guarded / Dangerous | low / high | moderate / severe | high / severe |

Roles against the base for their fight: skirmisher 60% HP, armour +1, d4, speed 6; brute 110% HP, armour −1, d10, speed 3; warden 175% HP, armour +1, +2 damage. Raider and archer are the base. Fights 1–2 are revision 8's boards, seeds and enemies exactly (only the archer's name and the preview text differ).

## Steps tried

| Step | Change (cumulative) | Rotation win | Defeats fights 3 / 4 / 5 | Victories with a death | HP taken, Dangerous: pressure / finale | Pressure layouts, rounds spread | Kept |
| --- | --- | --- | --- | --- | --- | --- | --- |
| before | revision 8 | 86.7% | 0.3 / 5.2 / 7.7% | 16.0% | 57% / 45% | (recipes 0–2) 14% | baseline |
| 0 | contract starting numbers (skirmisher 60%, brute 150%, warden 200% HP, +1) | 50.9% | 4.4 / 16.8 / 34.8% | 11.7% | 74% / 55% | 20% | no: far below 55% |
| 1 | brute 120%, warden 150% | 55.4% | 3.7 / 14.2 / 31.8% | 10.5% | 71% / 52% | 18% | no: melee party 43.8% |
| 2 | base 80% HP and damage +3 in fights 3–5 | 79.2% | 1.7 / 5.4 / 13.9% | 10.5% | 58% / 50% | 14% | no: Dangerous finale costs less than pressure; layouts alike |
| 3 | brute 110%, warden 175% +2, causeway block (8, 3..7) | 79.3% | 1.3 / 5.6 / 14.3% | 11.1% | 55% / 51% | 28% | no: Dangerous finale still costs less |
| **4** | **pressure base 75% HP, finale 90%** | **77.2%** | **0.9 / 5.0 / 17.5%** | **11.8%** | **53% / 56%** | **29%** | **yes** |
| 5 | skirmisher die = base − 2 | 76.7% | 1.1 / 5.3 / 17.7% | 11.5% | 54% / 56% | 28% | no: no gain, melee party 62.5% |
| 6 | step 4 with finale base 75% HP, damage +4 | 78.3% | 0.9 / 5.0 / 16.4% | 11.3% | 53% / 53% | 29% | no: Dangerous finale no costlier than pressure |
| 7 | step 4 with pressure base 70% HP, damage +4 | 73.9% | 1.5 / 6.1 / 19.4% | 10.8% | 56% / 54% | 28% | no: worse on every open target |

The share of victories that lose a hero stayed at 10.5–11.8% in every revision 9 step: the bigger rosters turn the close finales that cost one hero under revision 8 into wipes or clean wins (heroes still Downed when a won finale ends: 62 against 111 under revision 8).

## Targets

| # | Target | Before (revision 8) | After (revision 9) | Verdict |
| --- | --- | --- | --- | --- |
| E1 | Pacing rises by stage for each route; defeat share by fight rises from fight 3 | HP taken Guarded 17 / 24 / 23%, Dangerous 46 / 57 / 45%; net HP lost falls by stage; defeats 0.3 / 5.2 / 7.7% | **HP taken Guarded 17 / 27 / 41%, Dangerous 46 / 53 / 56%; defeats 0.9 / 5.0 / 17.5%**; net HP lost Guarded 12 / 10 / 17%, Dangerous 27 / 24 / 27% | **met** on HP taken and defeats; missed on net HP lost (pressure below approach) |
| E2 | Within a stage, layouts differ by at least 15% in rounds or melee share | approach recipes: rounds 35% apart | **approach 35% (4.25 to 5.72 rounds), pressure 29% (5.66 to 7.28)**; melee share 8% and 2% | **met** (on rounds) |
| E3 | A higher risk word costs more and loses more often, every pair | HP taken 17 / 24 / 38 / 53%, defeats 0.0 / 0.2 / 0.4 / 8.3% | **HP taken low 17%, moderate 27%, high 44%, severe 54%; defeats 0.0%, 1.0%, 2.8%, 11.9%**; net HP lost 12 / 10 / 24 / 25% | **met** on HP taken and defeats; missed on net HP lost for moderate vs low |
| E4 | Slice 3 targets hold | | | |
| | rotation win rate 55–90% | 86.7% | **77.2% (75.6–78.7%)** | met |
| | every representative party at least 35% | lowest 84.4% | **lowest 65.6% (melee)** | met |
| | at least 15% of victories lose a hero | 16.0% | **11.8%** | **missed** |
| | Dangerous at least 1.5x Guarded HP (net, as slice 3) | 2.3–5.9x | **1.6x (fight 5) to 3.3x (fight 4)** | met |
| | the finale has the highest defeat share | 7.7% | **17.5%** | met |
| | melee within 20 points of ranged; melee damage gap at most 33% | melee 90.6%, Fighters 100%, ranged 87.5%; gap 18% | **melee 65.6%, Fighters 90.6%, ranged 87.5%; gap 16%** | **missed narrowly** for the melee party (21.9 points; 32 runs each, intervals 48–80% and 72–95%) |
| E5 | Gates zero, no stalls, same seed same results | met | gates 0; same seed, same results (kept step rerun at the head, CSVs byte-identical); **2 stalls** | **missed** on stalls (2 of 3,168 runs) |

## Stages by route

| Stage | Route | Defeats before → after | Rounds before → after | HP taken before → after | HP lost (net) before → after | Melee share before → after |
| --- | --- | --- | --- | --- | --- | --- |
| approach | guarded | 0.0% → 0.0% | 3.6 → 3.6 | 17% → 17% | 12% → 12% | 30% → 30% |
| pressure | guarded | 0.2% → 1.0% | 4.8 → 4.9 | 24% → 27% | 7% → 10% | 30% → 35% |
| finale | guarded | 0.9% → 8.2% | 5.0 → 7.0 | 23% → 41% | 5% → 17% | 32% → 41% |
| approach | dangerous | 0.2% → 0.2% | 6.4 → 6.4 | 46% → 46% | 27% → 27% | 36% → 36% |
| pressure | dangerous | 5.3% → 4.9% | 7.7 → 7.7 | 57% → 53% | 27% → 24% | 39% → 39% |
| finale | dangerous | 14.9% → 27.1% | 8.3 → 10.1 | 45% → 56% | 19% → 27% | 38% → 42% |

## Layouts (after, revision 9)

| Recipe | Layout | Route | Fights (stages) | Defeats | Rounds | HP taken | HP lost | Melee share |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | split stones | guarded | 594 (1) | 0.0% | 3.0 | 14% | 10% | 28% |
| 0 | split stones | dangerous | 1089 (1, 2) | 0.1% | 6.1 | 43% | 25% | 33% |
| 1 | ruined pillars | guarded | 1187 (1, 2) | 0.0% | 3.0 | 11% | 9% | 32% |
| 1 | ruined pillars | dangerous | 1088 (1, 2) | 0.0% | 5.6 | 41% | 25% | 37% |
| 2 | broken wall | guarded | 1386 (1, 2) | 0.0% | 4.4 | 23% | 15% | 30% |
| 2 | broken wall | dangerous | 990 (1, 2) | 0.4% | 7.5 | 54% | 32% | 38% |
| 3 | fallen colonnade | guarded | 384 (4) | 1.3% | 4.7 | 24% | 6% | 36% |
| 3 | fallen colonnade | dangerous | 495 (3) | 0.6% | 7.1 | 55% | 26% | 38% |
| 4 | sunken causeway | guarded | 885 (3, 4) | 1.5% | 6.0 | 26% | 8% | 38% |
| 4 | sunken causeway | dangerous | 1382 (3, 4) | 3.2% | 8.1 | 50% | 22% | 37% |
| 5 | cairn field | guarded | 1866 (3, 4) | 0.7% | 4.5 | 28% | 11% | 34% |
| 5 | cairn field | dangerous | 1283 (3, 4) | 8.3% | 7.4 | 56% | 27% | 42% |
| 6 | vault sanctum | guarded | 1517 (5) | 8.2% | 7.0 | 41% | 17% | 41% |
| 6 | vault sanctum | dangerous | 1459 (5) | 27.1% | 10.1 | 56% | 27% | 42% |

Before (revision 8, each recipe at every stage): split stones 4.1 / 6.8 rounds (Guarded / Dangerous), ruined pillars 3.6 / 6.6, broken wall 5.0 / 8.4; melee share 30–39%. The seeds draw recipes unevenly (the colonnade is fight 4 on Guarded and fight 3 on Dangerous in this seed set), so a layout's row mixes its stages; the stage table above is the comparison by stage.

What the layouts do: the **sunken causeway** is the long chokepoint fight of its stage (6.0 / 8.1 rounds), the **cairn field** the short, bloody one (Dangerous defeats 8.3%), the **colonnade** between. Melee share does not separate them (37–38% pooled); the contract's "favours closing in" and "ranged duels" are not visible in this policy's damage split, whose heroes attack from where they stand.

## Parties (after)

| Party | Win rate before → after (95% CI after) | Rounds/fight before → after | Deaths/run before → after |
| --- | --- | --- | --- |
| `familiar` | 90.6% → 84.4% (68–93%) | 5.6 → 6.0 | 0.44 → 0.69 |
| `melee` | 90.6% → 65.6% (48–80%) | 6.6 → 7.0 | 0.56 → 1.47 |
| `ranged` | 87.5% → 87.5% (72–95%) | 5.4 → 5.7 | 0.59 → 0.62 |
| `support-poor` | 84.4% → 84.4% (68–93%) | 5.5 → 5.5 | 0.78 → 0.66 |
| `support-heavy` | 90.6% → 75.0% (58–87%) | 5.9 → 6.1 | 0.53 → 1.06 |
| `mixed` | 90.6% → 90.6% (76–97%) | 5.8 → 6.3 | 0.50 → 0.41 |
| `dup-fighter` | 100.0% → 90.6% (76–97%) | 6.5 → 7.1 | 0.25 → 0.62 |
| `dup-cleric` | 90.6% → 78.1% (61–89%) | 6.3 → 6.5 | 0.59 → 1.03 |
| `dup-wizard` | 87.5% → 68.8% (51–82%) | 5.3 → 5.5 | 0.69 → 1.28 |
| rotation (90 parties) | 86.7% → 77.2% (75.6–78.7%) | 5.8 → 6.2 | 0.69 → 1.01 |

By class (rotation), Fighter 83% and Artificer 82% are highest and Monk 70% lowest (82.4% before), so the Monk follow-up from #184 still stands.

## Compatibility

- New runs are revision 9. Revision 7 and 8 runs keep their own generation, enemies, previews and archer name for life; restore regenerates each fight with the run's own revision (`OlderRevisionsResumeWithTheirOwnEnemies`: revision 8 and 9 checkpoints at a pressure route, mid-battle and between battles resume exactly and continue exactly as the uninterrupted run). The revision 8 seed-42 golden is unchanged and the revision 8 before-run reproduces #186's numbers, so the revision 8 path is untouched.
- Revision 9 draws the generation random numbers in revision 8's order: fights 1–2 keep their boards, seeds and capture fixtures (`CompactRevision9SeedFingerprint`).
- SRD keeps revision 7 generation at every revision. Unknown revisions are refused by name and preserved; schema 5 is unchanged.

## Stalls

Two runs stall at round 60, both in fight 4 on the Dangerous sunken causeway: `rot2-03` seed 20261119 (route pattern 10) and `rot4-02` seed 20261124 (pattern 15); reproduce with `DND_BALANCE_TRACE=rot2-03:20261119:10 DND_BALANCE_REVISION=9` and `Balance.Trace`. In the first, the last hero (a Barbarian) stands at (7, 8) with the causeway wall below it and the bodies of the skirmisher, the raider and the archer on its other three sides; the brute at (6, 6) cannot reach a cell beside it, and the hero cannot move. Neither side can ever act, so the fight never ends: a real soft lock that a player could only leave through Return to menu. The cause is a standing rule (bodies occupy their cells, and nothing ends a fight in which neither side can reach the other); report 10's 50-round fight had the same cause. Revision 9 makes it reachable: four-foe rosters leave more bodies, and walled lanes give them pockets to close. Step 1 had two such stalls in the cairn field, with the contract's three-cell causeway block, so it is not one layout's geometry. A layout change could only move the odds; the fix belongs to the rules (bodies that do not block movement, or a stalemate rule) and is outside this slice's "enemy behaviour unchanged" boundary. It is follow-up 1.

## Follow-ups

1. **Body-blocked stalemates** (2 of 3,168 runs): decide between bodies that do not block movement and a stalemate rule, with the harness before and after; needs a contract because it changes movement or the battle's end.
2. **Losses without ending the run** (11.8% of victories lose a hero, target 15%): the policy's all-or-nothing finales. Candidates are the rescue loop and target choice in #187, which change how a close fight ends rather than how strong the enemies are.
3. **Melee party in the vault sanctum** (65.6%, 21.9 points behind ranged): the front-to-back walk to the warden; measure again with #187's targeting changes.
4. **Melee share does not separate the pressure layouts**: a layout-aware damage split needs a policy that positions, not this one.
5. **Presentation** (PR B and the graphics lead): foe looks from the saved Role; recipes 3–6 draw as their obstacle kind once the kit reads `FExpeditionRun::Encounter`.
