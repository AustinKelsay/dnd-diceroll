# Format 5 checkpoint fixtures

Real save format 5 files written by unmodified `staging` at `6ee589dfdd8c369879c40e9a6541727198a71b4c` (the merge of #214, catalog revision 9; Linux, UE 5.8.4, PlebDev Ultra) on 2026-11-13, using `capture-harness.cpp.txt` as a temporary automation test (`Project.Tools.Schema5Baseline`) in a detached worktree of that commit. Both revisions use seed 42 and run identity `schema5-fixture` with the tutorial party (Champion), and the scripted player of `ExpeditionTestSupport` (`FinishCombat`, reward to the first conscious survivor, the camp).

For each of catalog revision 8 (`-r8`) and 9 (`-r9`):

- `route-r?-v5.json`: after fight 1 on the Guarded road, its reward and the camp; route phase.
- `reward-r?-v5.json`: after fight 2 on the Dangerous road; reward phase, nothing claimed (the next Guarded road has secured supplies).
- `mid-battle-r?-v5.json`: fight 3 on the Guarded road after the enemies' opening turns and one party move (movement partly spent).
- `expected-<name>`: staging's own final checkpoint when that fixture's run continues with `FinishRunScripted`.

`Project.Expedition.Persistence.Schema5LoadsAsGearOnly` (#218) loads every fixture through the 5 → 6 step migration, requires every claim to read as Gear and no hero to hold Reserve or Honed, and requires the resumed run to continue exactly as staging continued it, including both random streams. `Project.Expedition.Run.RewardPreviewsFollowRevision` restores them to prove the revision 8 and 9 route wording is the base build's. Do not edit these files by hand.
