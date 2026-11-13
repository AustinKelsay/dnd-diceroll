# Vault contrast spot check at the round 3 head

`gallery --fixture pal-00-vault --fixture pal-05-vault --fixture pal-10-vault --fixture pal-15-vault --poses tools/capture/palette-poses.json --res 1920x1080`, `tools/figure_contrast.py` per class over the five default-zoom shots, against the Vault column of the canonical gate run of round 2 (`../canonical-contrast-palette-gate.md`).

| build | mean now | worst shot now | canonical Vault mean |
| --- | --- | --- | --- |
| cleric.life | 4.29 | 3.43 | 4.28 |
| wizard.abjuration | 3.86 | 2.53 | 3.85 |
| fighter.champion | 3.47 | 2.20 | 3.46 |
| rogue.thief | 2.78 | 2.05 | 2.77 |
| cleric.trickery | 3.85 | 2.92 | 3.83 |
| wizard.illusion | 3.30 | 2.17 | 3.29 |
| paladin.ancients | 3.61 | 2.93 | 3.61 |
| ranger.hunter | 3.57 | 2.88 | 3.56 |
| wizard.transmutation | 3.16 | 2.11 | 3.16 |
| druid.spores | 3.38 | 2.46 | 3.38 |
| bard.valor | 2.72 | 2.09 | 2.72 |
| artificer.armorer | 3.22 | 2.12 | 3.22 |
| barbarian.wild-magic | 2.92 | 2.24 | 2.92 |
| monk.astral-self | 2.91 | 2.12 | 2.91 |
| paladin.watchers | 3.85 | 2.53 | 3.85 |
| ranger.swarmkeeper | 3.06 | 2.67 | 3.06 |

Lowest mean 2.72 (floor 2.0), lowest worst shot 2.05 (floor 1.4); every build within 0.02 of its canonical mean.
