# Progression targets (contract 13 section 6)

3168 paired runs on 32 seeds; 10000 seed-cluster resamples (bootstrap seed 20261112).

- P1 (C2, gear = revision 9): **met**
- P2 (each single card best for 2+ classes): **missed** — best counts {'gear': 0, 'reserve': 0, 'honed': 0}
- P3 (largest gap ≤ 10 points): **met** — largest 4.0
- P4 (mixed rise ≤ 8 points, holds): **met** — rise -0.7 pts (-2.6 to +1.2)
- P5 (reserve specials in fights 4-5 ≥ 1.5x): **met** — ratio 3.95 (3.61 to 4.38)
- P6 (gates zero, stalls ≤ before): **missed** — stalls {'before': 0, 'gear': 0, 'reserve': 1, 'honed': 0, 'mixed': 1}
- Non-dominance: **no** — classes where each strategy is clearly beaten: {'gear': [], 'reserve': [], 'honed': ['bard', 'druid', 'monk', 'ranger']}
- Extra batch needed: **no**

## Rotation win rates

| Measurement | Win rate |
| --- | --- |
| before | 78.1% |
| gear | 78.1% |
| reserve | 77.9% |
| honed | 74.1% |
| mixed | 77.4% |

## P3 gaps (points, paired)

| Pair | d (95% CI) | Verdict |
| --- | --- | --- |
| gear-reserve | +0.2 (-1.4 to +1.8) | met |
| gear-honed | +4.0 (+2.2 to +5.9) | met |
| honed-reserve | -3.8 (-5.7 to -1.8) | met |

## P2 classes (rotation, points, paired)

| Class | Runs | gear − honed | gear − reserve | honed − reserve | Best |
| --- | --- | --- | --- | --- | --- |
| artificer | 640 | +4.5 (+1.4 to +7.8) small | +2.5 (-0.3 to +5.5) tie | -2.0 (-4.7 to +0.6) tie | none (gear, reserve possible) |
| barbarian | 672 | +3.9 (+0.9 to +7.0) small | +0.7 (-1.8 to +3.4) tie | -3.1 (-6.7 to +0.1) tie | none (gear, reserve possible) |
| bard | 640 | +5.2 (+1.2 to +9.4) clear | +1.9 (-0.9 to +5.2) tie | -3.3 (-7.8 to +1.1) tie | none (gear, reserve possible) |
| cleric | 1472 | +4.3 (+2.1 to +6.7) small | +0.1 (-2.7 to +2.9) tie | -4.2 (-6.7 to -1.8) small | none (gear, reserve possible) |
| druid | 800 | +3.1 (+0.1 to +6.0) small | -3.2 (-6.8 to -0.1) small | -6.4 (-9.5 to -3.2) clear | none (reserve possible) |
| fighter | 800 | +2.9 (+0.1 to +5.9) small | -0.1 (-2.6 to +2.5) tie | -3.0 (-6.8 to +0.4) tie | none (gear, reserve possible) |
| monk | 800 | +4.6 (+1.5 to +8.1) small | -0.4 (-3.5 to +2.5) tie | -5.0 (-8.2 to -1.8) clear | none (gear, reserve possible) |
| paladin | 800 | +2.6 (+0.6 to +4.8) small | -0.4 (-3.2 to +2.5) tie | -3.0 (-6.1 to -0.1) small | none (gear, reserve possible) |
| ranger | 640 | +3.8 (+0.8 to +6.9) small | -1.2 (-3.9 to +1.2) tie | -5.0 (-8.0 to -2.0) clear | none (gear, reserve possible) |
| rogue | 864 | +4.2 (+2.0 to +6.5) small | -0.7 (-4.1 to +2.4) tie | -4.9 (-8.2 to -2.0) small | none (gear, reserve possible) |
| sorcerer | 608 | +3.9 (+0.5 to +7.6) small | +3.6 (+0.2 to +7.4) small | -0.3 (-3.8 to +3.1) tie | none (gear possible) |
| warlock | 768 | +3.3 (+0.7 to +5.9) small | -0.8 (-4.2 to +2.7) tie | -4.0 (-7.9 to -0.3) small | none (gear, reserve possible) |
| wizard | 1536 | +4.2 (+1.8 to +6.6) small | +0.1 (-1.9 to +2.2) tie | -4.1 (-6.2 to -2.0) small | none (gear, reserve possible) |

## P4 holds (mixed)

- Rotation 55–90%: 77.4% (75.9–78.9%) met
- Every representative party ≥ 35%: met (dup-cleric 65%, dup-fighter 84%, dup-wizard 65%, familiar 84%, melee 58%, mixed 65%, ranged 61%, support-heavy 58%, support-poor 45% lower bounds)
- Dangerous ≥ 1.5x Guarded net HP lost, per fight: met (fight 1 2.31x [1.97, 2.75] met, 0 sign-decided, fight 2 2.38x [1.93, 2.98] met, 0 sign-decided, fight 3 2.37x [2.00, 2.82] met, 0 sign-decided, fight 4 3.96x [3.10, 5.40] met, 0 sign-decided, fight 5 1.84x [1.55, 2.18] met, 0 sign-decided)
- Finale has the highest defeat share: met (vs fight 1 +17.1 [+12.4, +22.2], vs fight 2 +17.0 [+12.4, +22.2], vs fight 3 +15.8 [+11.2, +20.9], vs fight 4 +12.4 [+8.3, +16.9])
