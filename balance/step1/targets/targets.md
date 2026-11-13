# Progression targets (contract 13 section 6)

3168 paired runs on 32 seeds; 10000 seed-cluster resamples (bootstrap seed 20261112).

- P1 (C2, gear = revision 9): **met**
- P2 (each single card best for 2+ classes): **missed** — best counts {'gear': 0, 'reserve': 0, 'honed': 0}
- P3 (largest gap ≤ 10 points): **met** — largest 3.8
- P4 (mixed rise ≤ 8 points, holds): **met** — rise -0.6 pts (-2.7 to +1.3)
- P5 (reserve specials in fights 4-5 ≥ 1.5x): **met** — ratio 3.95 (3.61 to 4.38)
- P6 (gates zero, stalls ≤ before): **missed** — stalls {'before': 0, 'gear': 0, 'reserve': 1, 'honed': 0, 'mixed': 1}
- Non-dominance: **no** — classes where each strategy is clearly beaten: {'gear': [], 'reserve': [], 'honed': ['druid', 'ranger']}
- Extra batch needed: **no**

## Rotation win rates

| Measurement | Win rate |
| --- | --- |
| before | 78.1% |
| gear | 78.1% |
| reserve | 77.9% |
| honed | 74.3% |
| mixed | 77.5% |

## P3 gaps (points, paired)

| Pair | d (95% CI) | Verdict |
| --- | --- | --- |
| gear-reserve | +0.2 (-1.4 to +1.8) | met |
| gear-honed | +3.8 (+2.0 to +5.8) | met |
| honed-reserve | -3.6 (-5.5 to -1.8) | met |

## P2 classes (rotation, points, paired)

| Class | Runs | gear − honed | gear − reserve | honed − reserve | Best |
| --- | --- | --- | --- | --- | --- |
| artificer | 640 | +4.2 (+0.9 to +8.0) small | +2.5 (-0.3 to +5.5) tie | -1.7 (-5.0 to +1.4) tie | none (gear, reserve possible) |
| barbarian | 672 | +3.7 (+0.4 to +7.1) small | +0.7 (-1.8 to +3.4) tie | -3.0 (-6.2 to +0.0) tie | none (gear, reserve possible) |
| bard | 640 | +4.5 (+0.6 to +8.8) small | +1.9 (-0.9 to +5.2) tie | -2.7 (-7.0 to +1.6) tie | none (gear, reserve possible) |
| cleric | 1472 | +4.3 (+1.8 to +6.9) small | +0.1 (-2.7 to +2.9) tie | -4.1 (-6.4 to -2.0) small | none (gear, reserve possible) |
| druid | 800 | +3.5 (+0.7 to +6.2) small | -3.2 (-6.8 to -0.1) small | -6.8 (-9.6 to -3.9) clear | none (reserve possible) |
| fighter | 800 | +3.5 (+0.2 to +6.8) small | -0.1 (-2.6 to +2.5) tie | -3.6 (-7.4 to -0.1) small | none (gear, reserve possible) |
| monk | 800 | +3.8 (+0.6 to +7.2) small | -0.4 (-3.5 to +2.5) tie | -4.1 (-7.0 to -1.2) small | none (gear, reserve possible) |
| paladin | 800 | +2.0 (-0.2 to +4.4) tie | -0.4 (-3.2 to +2.5) tie | -2.4 (-5.5 to +0.5) tie | none (gear, reserve, honed possible) |
| ranger | 640 | +4.1 (+1.2 to +7.0) small | -1.2 (-3.9 to +1.2) tie | -5.3 (-8.1 to -2.5) clear | none (gear, reserve possible) |
| rogue | 864 | +4.3 (+1.9 to +6.8) small | -0.7 (-4.1 to +2.4) tie | -5.0 (-8.4 to -2.0) small | none (gear, reserve possible) |
| sorcerer | 608 | +4.3 (+0.8 to +7.9) small | +3.6 (+0.2 to +7.4) small | -0.7 (-3.8 to +2.5) tie | none (gear possible) |
| warlock | 768 | +2.7 (+0.3 to +5.3) small | -0.8 (-4.2 to +2.7) tie | -3.5 (-7.2 to +0.0) tie | none (gear, reserve possible) |
| wizard | 1536 | +4.2 (+1.7 to +6.6) small | +0.1 (-1.9 to +2.2) tie | -4.0 (-6.2 to -1.9) small | none (gear, reserve possible) |

## P4 holds (mixed)

- Rotation 55–90%: 77.5% (75.9–79.0%) met
- Every representative party ≥ 35%: met (dup-cleric 65%, dup-fighter 84%, dup-wizard 65%, familiar 84%, melee 58%, mixed 65%, ranged 61%, support-heavy 58%, support-poor 51% lower bounds)
- Dangerous ≥ 1.5x Guarded net HP lost, per fight: met (fight 1 2.31x [1.97, 2.75] met, 0 sign-decided, fight 2 2.38x [1.93, 2.98] met, 0 sign-decided, fight 3 2.37x [2.00, 2.81] met, 0 sign-decided, fight 4 3.97x [3.11, 5.43] met, 0 sign-decided, fight 5 1.85x [1.55, 2.20] met, 0 sign-decided)
- Finale has the highest defeat share: met (vs fight 1 +17.0 [+12.3, +22.3], vs fight 2 +17.0 [+12.2, +22.3], vs fight 3 +15.8 [+11.1, +20.9], vs fight 4 +12.4 [+8.3, +16.8])
