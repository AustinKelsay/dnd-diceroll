# Slice 3 part 2 report: Compact tuning, before and after

Child issue [#184](https://github.com/AustinKelsay/dnd-diceroll/issues/184), parent [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112). Contract: [09-balance-tuning-contract.md](09-balance-tuning-contract.md). Before: [08-balance-baseline-report.md](08-balance-baseline-report.md).

## How it was measured

- Same harness, policy (version 1), 99 parties and 32 seeds as the baseline: 3,168 complete Compact runs per measurement.
- **Before:** staging `bd02ede`, catalog revision 7; the CSVs are identical to the #180 evidence.
- **After:** this branch, catalog revision 8. Each tried step was a full rebuild and a full `Balance.Baseline.` run under the machine lock at `--cores 2`. The kept step's results are byte-identical when rerun at the PR head.
- Gates in every step: 0 policy errors, illegal proposals, passive ends, protection mismatches and stalls.
- Read every number as "what this simple policy does", not as human difficulty: a player who retreats, saves resources and positions should do better.

## What changed

Only lever 1 of the contract: **Compact enemy stats**. With s = 0 to 4 for fights 1 to 5:

| | HP | Armour | Attack | Damage |
| --- | --- | --- | --- | --- |
| Guarded (2 enemies, one an archer) | 20 + 4s (was 10 + 2s) | 12 (was 11) | +4, +4, +5, +5, +5 (was +2) | d6 + 3, 3, 3, 4, 4 (was d4 + 1) |
| Dangerous (3 enemies, one an archer) | 24 + 4s (was 12 + 2s) | 13 (was 12) | +4, +4, +5, +5, +5 (was +2) | d8 + 3, 3, 3, 4, 4 (was d6 + 1) |

All of these stay within the contract's bounds: HP at most double, attack at most +5, die at most d8. Counts, roles, ranges, grids, spawns, previews, rewards and camp are unchanged. SRD runs keep the revision 7 numbers.

Lever 2 (Compact melee speed 5) was **not applied**: the contract allows it only if the melee target still fails after lever 1, and it passes (T6 below).

## Steps tried

| Step | Enemy stats (fight s = 0–4) | Rotation win rate | Victories with a death | Fight 5 low point, Guarded / Dangerous | Defeats in fights 4 / 5 | Melee damage gap | Kept |
| --- | --- | --- | --- | --- | --- | --- | --- |
| before | HP 10 / 12 + 2s, AC 11 / 12, +2, d4 / d6 + 1 (revision 7) | 100.0% | 0.6% | 77% / 73% | 0.0% / 0.0% | 33% | baseline |
| 1 | HP 16 / 18 + 4s, AC 12 / 13, +3 rising to +5, d6 / d8 + 2 | 98.6% | 8.1% | 56% / 45% | 0.3% / 1.0% | 21% | no: still 98.6% wins |
| 2 | HP 20 / 24 + 4s, AC 12 / 13, +4 rising to +5, d6 / d8 + 3 | 89.5% | 15.0% | 40% / 28% | 4.2% / 6.0% | 18% | no: 89.5%, at the band's edge |
| 3 | as 2, damage bonus 3 rising to 4 from fight 4 | 86.7% | 16.1% | 37% / 24% | 5.2% / 7.7% | 18% | **yes** |
| 4 | as 3, Guarded also d8 | 85.7% | 17.2% | 34% / 23% | 5.8% / 8.2% | 18% | no: blurs Guarded vs Dangerous for 1 point |

## Targets

| # | Target (proposal) | Before | After | Verdict |
| --- | --- | --- | --- | --- |
| T1 | Rotation win rate 55–90%; every representative party at least 35% | 100%; all 100% | **86.7% (85.5–87.9%)**; lowest party `support-poor` 84.4% | met, but near the top of the band |
| T2 | At least 15% of victories lose a hero | 0.6% | **16.1%** | met, narrowly |
| T3 | Fights 4–5 mean low point below 60%; a quarter of fight-5 fights below half | 73–82%; 0% below half | **24–50%; 74% (Guarded) and 92% (Dangerous) of fight-5 fights below half** | met |
| T4 | Dangerous costs at least 1.5x Guarded HP and loses more often | 2–4x; no losses | **2.3–5.9x; Dangerous defeats higher at every stage** | met |
| T5 | The finale has the highest defeat share | none | **0.1%, 0.1%, 0.3%, 5.2%, 7.7%** by fight | met |
| T6 | Melee parties within 20 points of the ranged party; melee damage gap no wider than 33% | all 100%; gap 33% | **melee 90.6%, Fighter duplicates 100%, ranged 87.5%; gap 18%** | met |
| T7 | Specials through fight 5 (reported only) | 4.9 / 3.7 / 0.9 / 0.4 / 0.25 per fight | 6.3 / 3.8 / 0.8 / 0.5 / 0.2 | not addressed (deferred, contract section 3) |
| T8 | Gates zero; same seed, same results | met | **met** | met |

## Parties and archetypes

| Party | Archetype | Win rate before | Win rate after (95% CI) | Rounds/fight before → after | Deaths/run before → after | Potions/run before → after |
| --- | --- | --- | --- | --- | --- | --- |
| `familiar` | mixed | 100% | 90.6% (75.8%–96.8%) | 3.1 → 5.6 | 0.00 → 0.44 | 1.2 → 8.9 |
| `melee` | melee | 100% | 90.6% (75.8%–96.8%) | 3.9 → 6.6 | 0.00 → 0.56 | 2.0 → 10.0 |
| `ranged` | ranged | 100% | 87.5% (71.9%–95.0%) | 2.6 → 5.4 | 0.00 → 0.59 | 1.8 → 10.2 |
| `support-poor` | support-poor | 100% | 84.4% (68.2%–93.1%) | 3.0 → 5.5 | 0.03 → 0.78 | 2.4 → 9.9 |
| `support-heavy` | support-heavy | 100% | 90.6% (75.8%–96.8%) | 3.1 → 5.9 | 0.00 → 0.53 | 1.3 → 9.6 |
| `mixed` | mixed | 100% | 90.6% (75.8%–96.8%) | 3.2 → 5.8 | 0.00 → 0.50 | 0.9 → 9.6 |
| `dup-fighter` | duplicate | 100% | 100.0% (89.3%–100.0%) | 4.0 → 6.5 | 0.00 → 0.25 | 0.8 → 8.8 |
| `dup-cleric` | duplicate | 100% | 90.6% (75.8%–96.8%) | 3.3 → 6.3 | 0.03 → 0.59 | 0.6 → 9.7 |
| `dup-wizard` | duplicate | 100% | 87.5% (71.9%–95.0%) | 2.3 → 5.3 | 0.00 → 0.69 | 2.0 → 10.4 |
| rotation (90 parties) | rotation | 100% | 86.7% (85.4%–87.9%) | 3.1 → 5.8 | 0.01 → 0.69 | 1.9 → 10.0 |

Potions matter now: the party drinks about 10 per run (it is given about 10), against fewer than 2 before.

## Encounters by stage and route

| Fight | Route | Defeats before → after | Lowest party HP, mean, before → after | Fights below half HP before → after | HP lost per fight before → after |
| --- | --- | --- | --- | --- | --- |
| 1 | guarded | 0.0% → 0.0% | 97% → 87% | 0% → 0% | 3% → 11% |
| 1 | dangerous | 0.0% → 0.1% | 90% → 69% | 0% → 5% | 9% → 26% |
| 2 | guarded | 0.0% → 0.0% | 96% → 82% | 0% → 0% | 4% → 12% |
| 2 | dangerous | 0.0% → 0.2% | 89% → 65% | 0% → 12% | 9% → 28% |
| 3 | guarded | 0.0% → 0.1% | 87% → 62% | 0% → 18% | 5% → 10% |
| 3 | dangerous | 0.0% → 0.4% | 83% → 49% | 0% → 48% | 8% → 24% |
| 4 | guarded | 0.0% → 0.3% | 82% → 50% | 0% → 45% | 2% → 5% |
| 4 | dangerous | 0.0% → 10.2% | 76% → 30% | 0% → 85% | 9% → 29% |
| 5 | guarded | 0.0% → 0.9% | 77% → 37% | 0% → 74% | 2% → 5% |
| 5 | dangerous | 0.0% → 14.9% | 73% → 24% | 2% → 92% | 5% → 19% |

Fights now last 5.8 rounds on average (3.1 before). 132 of 15,640 fights last 15 rounds or more, 18 last 20 or more, and the longest lasts 50 (below).

## Classes (rotation)

| Class | Win rate before → after | Hero death rate before → after | Damage/fight after | Taken/fight before → after | Healing/fight after |
| --- | --- | --- | --- | --- | --- |
| paladin | 100% → 90.2% (88.0%–92.1%) | 0.0% → 13.0% | 15.7 | 1.7 → 8.1 | 4.7 |
| fighter | 100% → 89.1% (86.8%–91.1%) | 0.0% → 15.9% | 15.8 | 2.1 → 10.3 | 5.0 |
| artificer | 100% → 88.6% (85.9%–90.8%) | 0.2% → 13.8% | 19.6 | 1.7 → 7.6 | 4.3 |
| druid | 100% → 87.9% (85.4%–90.0%) | 0.1% → 14.4% | 18.5 | 1.9 → 8.0 | 4.6 |
| barbarian | 100% → 87.6% (84.9%–89.9%) | 0.1% → 17.3% | 17.8 | 2.5 → 10.9 | 4.0 |
| warlock | 100% → 87.0% (84.4%–89.2%) | 0.0% → 17.1% | 20.3 | 1.8 → 7.5 | 3.9 |
| wizard | 100% → 87.0% (85.2%–88.6%) | 0.0% → 16.7% | 19.8 | 1.9 → 7.8 | 4.2 |
| rogue | 100% → 86.9% (84.5%–89.0%) | 0.6% → 16.3% | 21.1 | 2.6 → 9.5 | 4.4 |
| cleric | 100% → 86.3% (84.4%–87.9%) | 0.4% → 18.0% | 19.1 | 2.1 → 8.4 | 4.8 |
| ranger | 100% → 86.1% (83.2%–88.6%) | 0.0% → 16.4% | 21.1 | 1.6 → 6.5 | 3.9 |
| bard | 100% → 84.1% (81.0%–86.7%) | 0.0% → 20.0% | 19.4 | 2.4 → 9.3 | 4.3 |
| sorcerer | 100% → 83.9% (80.7%–86.6%) | 0.2% → 18.8% | 18.4 | 1.9 → 7.2 | 4.6 |
| monk | 100% → 82.4% (79.6%–84.9%) | 0.2% → 25.8% | 15.4 | 2.9 → 10.5 | 4.2 |

Hero death rate by slot: slot 1 22.7%, slot 2 16.3%, slot 3 15.2%, slot 4 14.5% (slot 1 still takes the most hits; enemy target choice is the deferred follow-up).

## What the tuned baseline now shows

- **Win rate can rank kits and classes now.** The rotation's kit win rates run from 77.5% (sorcerer.draconic-bloodline) to 92.5% (paladin.devotion, paladin.vengeance), and by class from Monk 82.4% (hero death rate 25.8%) to Paladin 90.2% (13.0%). No kit is a robust-z or Wilson outlier on win rate. These are the inputs for slice 4 and any class follow-up, not changes made here.
- **Fragile front-liners:** Monks (HP 24, AC 14, melee) take 10.5 HP per fight and die most (25.8%). Fighters (HP 30, AC 16) take about as much (10.3) and die less (15.9%), and Paladins (HP 28, AC 16) take 8.1 and die least (13.0%). Monk HP or AC is a candidate for a follow-up.
- **Deaths:** 2,138 (0.67 per run): 1,103 in wipes, 609 at the rescue deadline and 426 still Downed when a fight was won. The last two are partly the policy, which does not hold the last kill to save a Downed ally.
- **Special signatures:** 41 flags against 51 before. With real pressure, heals and protection are used more. Healing per fight is now about 4–7 HP per hero, potions included, with the Life and Peace clerics highest (7.1 and 7.3).
- **Longest fight, a policy artifact with a real cause:** `rot3-16:20261137:28`, fight 5, 50 rounds (reproduce with `Balance.Trace`). A 1-HP rogue keeps rescuing a Downed ally, because the policy revives before it attacks, and the archer re-downs that ally each round. Meanwhile a 2-HP raider stands beside them but cannot reach the rogue, since bodies and walls block every cell next to it. A player would shoot the raider. The real-game note: Rescue restores 1 HP, so a ranged enemy can re-down the same hero every round.

## Compatibility

- **Catalog revision 7 → 8.** A checkpoint saved under revision 7 (Compact or SRD) is refused with "This expedition was saved under rules revision 7; this build plays revision 8, so it cannot be resumed." The file stays on disk, and New Run moves it aside as before.
- **Format-4 fixtures:** they are revision 7, so they are now proven refused by name and left byte-identical. The 4→5 upgrade is proven on current runs written as format 4, which decode, restore and continue identically.
- Schema 5 is unchanged.
- **SRD:** an SRD seed-42 golden, captured with the same test on revision 7 code, still holds (`Project.Expedition.Run.SrdSeedUnchangedByCompactTuning`).

## Follow-ups (not in this PR)

1. **Special economy across five fights** (T7): Kit and Rage caps, camp allowance, Rage duration and gating, and touch-range specials. These need a Compact view of the catalog's resource facts that the HUD reads, and agreement with #95 for any shared number.
2. **Enemy target choice** (slot 1 takes 22.7% hero deaths against 14.5–16.3% for the other slots): an enemy-behaviour contract.
3. **Monk survivability**, measured against these tables.
4. **Rescue at 1 HP** against ranged enemies: consider in slice 4 or progression.
5. **Policy:** an optional "finish a low enemy before reviving" refinement would remove the long rescue loops. That would be policy version 2, measured against both rule revisions.

## Appendix: every kit after tuning (rotation, 160 or 192 hero-runs each)

| Kit | Win rate after (95% CI) | Hero death rate after | Damage/fight after | Healing/fight after | Protection/fight after | Special 1 / 2 uses per run after |
| --- | --- | --- | --- | --- | --- | --- |
| artificer.alchemist | 90.6% (85.1%–94.2%) | 11.9% | 18.8 | 5.1 | 0.00 | 0.75 / 2.23 |
| artificer.armorer | 88.1% (82.2%–92.3%) | 15.6% | 18.9 | 4.1 | 0.70 | 2.90 / 0.10 |
| artificer.artillerist | 86.2% (80.1%–90.7%) | 15.6% | 19.0 | 4.1 | 0.19 | 1.04 / 1.95 |
| artificer.battle-smith | 89.4% (83.6%–93.3%) | 11.9% | 21.6 | 3.8 | 0.03 | 0.18 / 2.33 |
| barbarian.beast | 85.4% (79.7%–89.7%) | 20.8% | 18.2 | 4.0 | 0.16 | 2.99 / 1.47 |
| barbarian.berserker | 90.0% (84.4%–93.8%) | 18.1% | 18.6 | 3.7 | 0.20 | 2.98 / 2.12 |
| barbarian.totem-warrior | 86.2% (80.1%–90.7%) | 17.5% | 17.0 | 4.2 | 0.31 | 2.99 / 1.14 |
| barbarian.wild-magic | 89.4% (83.6%–93.3%) | 11.9% | 17.4 | 4.0 | 0.15 | 3.00 / 2.23 |
| bard.creation | 85.6% (79.4%–90.2%) | 19.4% | 19.3 | 4.2 | 0.00 | 0.99 / 1.53 |
| bard.eloquence | 82.5% (75.9%–87.6%) | 20.6% | 20.7 | 4.0 | 0.02 | 0.34 / 2.64 |
| bard.lore | 83.1% (76.6%–88.1%) | 23.8% | 19.3 | 5.0 | 0.00 | 2.14 / 0.77 |
| bard.valor | 85.0% (78.7%–89.7%) | 16.2% | 18.1 | 4.3 | 0.31 | 1.68 / 1.31 |
| cleric.knowledge | 88.8% (82.9%–92.8%) | 16.9% | 19.4 | 4.0 | 0.00 | 0.18 / 2.80 |
| cleric.life | 83.3% (77.4%–87.9%) | 19.8% | 17.6 | 7.1 | 0.30 | 1.33 / 1.64 |
| cleric.light | 83.8% (77.3%–88.7%) | 18.8% | 18.1 | 4.2 | 0.38 | 0.86 / 2.12 |
| cleric.nature | 90.0% (84.4%–93.8%) | 15.0% | 19.1 | 5.9 | 0.00 | 1.04 / 1.34 |
| cleric.order | 86.9% (80.8%–91.3%) | 18.8% | 18.8 | 3.8 | 0.60 | 0.03 / 2.97 |
| cleric.peace | 88.8% (82.9%–92.8%) | 14.4% | 16.9 | 7.3 | 0.00 | 2.16 / 0.06 |
| cleric.tempest | 83.8% (77.3%–88.7%) | 18.8% | 21.8 | 3.8 | 0.00 | 1.08 / 1.29 |
| cleric.trickery | 86.9% (80.8%–91.3%) | 20.0% | 19.0 | 4.1 | 0.95 | 2.93 / 0.07 |
| cleric.twilight | 83.1% (76.6%–88.1%) | 22.5% | 20.6 | 3.8 | 0.10 | 0.53 / 2.11 |
| cleric.war | 88.1% (82.2%–92.3%) | 14.4% | 20.4 | 4.1 | 0.06 | 1.45 / 0.65 |
| druid.land | 87.5% (81.5%–91.8%) | 13.8% | 18.1 | 4.0 | 0.26 | 0.42 / 2.56 |
| druid.moon | 86.2% (80.1%–90.7%) | 16.9% | 19.6 | 4.1 | 0.79 | 2.83 / 0.17 |
| druid.spores | 89.4% (83.6%–93.3%) | 12.5% | 17.6 | 5.7 | 0.00 | 0.82 / 1.11 |
| druid.stars | 88.8% (82.9%–92.8%) | 15.6% | 19.1 | 5.3 | 0.00 | 2.12 / 0.88 |
| druid.wildfire | 87.5% (81.5%–91.8%) | 13.1% | 17.7 | 4.1 | 0.00 | 2.10 / 0.59 |
| fighter.battle-master | 91.9% (86.6%–95.2%) | 12.5% | 17.1 | 6.5 | 0.00 | 1.98 / 2.00 |
| fighter.champion | 90.1% (85.1%–93.6%) | 16.7% | 15.6 | 6.4 | 0.00 | 2.05 |
| fighter.eldritch-knight | 88.5% (83.3%–92.3%) | 16.1% | 15.3 | 4.0 | 0.56 | 2.48 / 0.51 |
| fighter.psi-warrior | 89.4% (83.6%–93.3%) | 13.8% | 16.1 | 4.0 | 0.39 | 2.29 / 0.70 |
| fighter.rune-knight | 84.4% (78.0%–89.2%) | 20.0% | 15.2 | 3.9 | 0.35 | 1.73 / 1.27 |
| monk.astral-self | 83.1% (76.6%–88.1%) | 23.1% | 14.6 | 4.1 | 0.74 | 0.14 / 2.86 |
| monk.four-elements | 81.2% (74.5%–86.5%) | 28.1% | 16.5 | 3.9 | 0.00 | 2.86 / 0.07 |
| monk.mercy | 78.8% (71.8%–84.4%) | 30.0% | 15.3 | 4.9 | 0.00 | 0.52 / 2.32 |
| monk.open-hand | 85.6% (79.4%–90.2%) | 25.6% | 15.6 | 4.2 | 0.40 | 1.56 / 1.43 |
| monk.shadow | 81.2% (75.1%–86.1%) | 22.9% | 15.2 | 4.0 | 0.24 | 0.59 / 1.60 |
| paladin.ancients | 88.8% (82.9%–92.8%) | 14.4% | 14.9 | 4.0 | 0.21 | 0.72 / 2.24 |
| paladin.devotion | 92.5% (87.3%–95.7%) | 13.1% | 15.9 | 5.5 | 0.37 | 1.14 / 1.15 |
| paladin.glory | 87.5% (82.1%–91.5%) | 15.6% | 13.8 | 5.9 | 0.00 | 0.60 / 1.28 |
| paladin.vengeance | 92.5% (87.3%–95.7%) | 11.9% | 18.2 | 3.8 | 0.00 | 2.71 / 0.19 |
| paladin.watchers | 91.2% (85.8%–94.7%) | 9.4% | 15.8 | 4.1 | 0.28 | 0.23 / 2.77 |
| ranger.beast-master | 85.0% (78.7%–89.7%) | 17.5% | 20.4 | 4.0 | 0.57 | 0.01 / 2.98 |
| ranger.fey-wanderer | 86.9% (80.8%–91.3%) | 17.5% | 21.6 | 3.7 | 0.00 | 0.21 / 2.79 |
| ranger.hunter | 86.2% (80.1%–90.7%) | 16.2% | 21.6 | 3.8 | 0.00 | 0.07 / 2.92 |
| ranger.swarmkeeper | 87.0% (81.5%–91.0%) | 14.6% | 20.7 | 4.2 | 0.00 | 1.18 / 1.81 |
| rogue.arcane-trickster | 88.5% (83.3%–92.3%) | 15.6% | 19.4 | 4.1 | 0.75 | 2.98 / 0.01 |
| rogue.assassin | 85.0% (78.7%–89.7%) | 17.5% | 21.6 | 4.0 | 0.00 | 1.64 / 0.33 |
| rogue.phantom | 87.5% (81.5%–91.8%) | 14.4% | 20.8 | 4.1 | 0.23 | 0.07 / 2.91 |
| rogue.soulknife | 87.0% (81.5%–91.0%) | 16.1% | 22.8 | 4.0 | 0.00 | 2.86 / 0.12 |
| rogue.thief | 86.2% (80.1%–90.7%) | 18.1% | 20.9 | 5.7 | 0.00 | 0.44 / 1.54 |
| sorcerer.aberrant-mind | 90.0% (84.4%–93.8%) | 13.8% | 18.9 | 4.0 | 0.30 | 0.99 / 2.01 |
| sorcerer.clockwork-soul | 85.0% (78.7%–89.7%) | 16.2% | 17.4 | 5.9 | 0.50 | 1.58 / 1.41 |
| sorcerer.draconic-bloodline | 77.5% (70.4%–83.3%) | 24.4% | 18.4 | 4.1 | 0.47 | 0.22 / 2.77 |
| sorcerer.wild-magic | 82.5% (75.9%–87.6%) | 20.6% | 18.7 | 4.2 | 0.00 | 2.79 / 0.20 |
| warlock.archfey | 88.8% (82.9%–92.8%) | 18.1% | 20.8 | 3.5 | 0.00 | 2.15 / 0.48 |
| warlock.fathomless | 89.4% (83.6%–93.3%) | 15.0% | 19.7 | 3.9 | 0.58 | 0.25 / 2.75 |
| warlock.fiend | 81.9% (75.2%–87.1%) | 23.1% | 19.9 | 4.3 | 0.00 | 2.60 / 0.39 |
| warlock.genie | 85.0% (78.7%–89.7%) | 18.1% | 20.1 | 3.8 | 0.12 | 1.23 / 0.93 |
| warlock.great-old-one | 90.1% (85.1%–93.6%) | 12.0% | 21.1 | 3.8 | 0.04 | 0.20 / 2.80 |
| wizard.abjuration | 90.6% (85.1%–94.2%) | 11.9% | 19.8 | 3.8 | 0.86 | 2.99 / 0.01 |
| wizard.bladesinging | 87.5% (81.5%–91.8%) | 17.5% | 19.2 | 4.0 | 0.12 | 1.18 / 1.14 |
| wizard.conjuration | 86.9% (80.8%–91.3%) | 18.1% | 19.1 | 3.9 | 0.00 | 0.68 / 1.92 |
| wizard.divination | 83.1% (76.6%–88.1%) | 20.0% | 20.0 | 3.9 | 0.37 | 0.91 / 2.08 |
| wizard.enchantment | 84.4% (78.0%–89.2%) | 19.4% | 19.2 | 4.4 | 0.00 | 1.83 / 0.71 |
| wizard.evocation | 83.8% (77.3%–88.7%) | 18.1% | 21.8 | 4.0 | 0.00 | 1.51 / 1.48 |
| wizard.illusion | 88.1% (82.2%–92.3%) | 14.4% | 19.4 | 3.9 | 0.57 | 2.99 / 0.01 |
| wizard.necromancy | 90.6% (85.1%–94.2%) | 17.5% | 20.2 | 4.3 | 0.00 | 2.44 / 0.54 |
| wizard.order-of-scribes | 90.6% (85.1%–94.2%) | 11.9% | 19.7 | 4.0 | 0.00 | 2.99 / 0.01 |
| wizard.transmutation | 85.0% (78.7%–89.7%) | 18.1% | 19.6 | 5.7 | 0.00 | 0.65 / 1.36 |
