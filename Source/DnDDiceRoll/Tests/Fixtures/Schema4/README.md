# Format 4 checkpoint fixtures

Real save format 4 files written by unmodified `staging` at `d9eac886688dfb487d07a0dd68b9a146e689ffcd` (Windows/UE 5.8.4, Amaterasu), using `capture-harness.cpp.txt` as a temporary automation test in a detached worktree. Seed 42, run identity `seeded-fixture`, the same policy as `Project.Expedition.Run.CompactSeedIdenticalFingerprint`.

- `mid-battle-v4.json`: encounter 1, combat phase, after enemy turns and one party move (movement partly spent).
- `between-battles-v4.json`: after encounter 1, reward claimed and camp recovery taken; route phase.
- `expected-mid-victory-v4.json`, `expected-victory-v4.json`: staging's own final checkpoints when each fixture's run continues to Victory.

`Project.Expedition.Persistence.Schema4LoadsAsCompact` loads both through the 4→5 upgrade and requires the resumed run to equal staging's continuation exactly, including RNG state. The same staging run produced the Compact golden fingerprint `rng=-958489057|partyHp=82|enemyHp=0|alive=4|phase=4|victories=5|ruleset=0|rounds=4|turn=4`. Do not edit these files by hand.
