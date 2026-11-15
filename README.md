# Evidence for #240 M1 (PR #258, head 50f5dfc2, base staging 1b6b79e4)

Host: PlebDev Ultra (Linux), UE 5.8.4 (CL 58902052). Every Unreal job ran under the machine lock at `--cores 2`.

- `balance/`: one folder per measurement, each with `summary.md`/`summary.json` (`tools/balance_report.py`) and each group's `group.json`. The raw CSVs stayed in `.local/` (12 measurements × 3,168 runs).
  - `m1-r10-before-<strategy>`: revision 10 on unmodified staging `5223a0b4` (policy 2).
  - `m1-r10-after-<strategy>`: revision 10 on this branch (policy 3).
  - `m1-r11-<strategy>`: revision 11 on this branch.
  - `m1_deltas.py` (`identity` and `deltas`) produced `m1-comparison.txt` and `m1-deltas.json`. The identity check found every CSV in all 24 groups byte-identical, before against after.
  - Sweeps ran on the branch before its rebase onto `1b6b79e4`. #255 between them adds only the read-only `FJourneyQuery`; no rules changed.
- `pin-digests-staging-5223a0b4.txt`: the six revision 10 digests `Project.Balance.Revision10RunsArePinned` pins, captured on unmodified staging.
- `gallery/`: `tools/dev.py gallery --fixture move-through-battle --res 1920x1080 --poses move-through-poses.json` (JPEG q85 of the PNGs, with the manifest and index). In `targeting-yaw0-near` and `targeting-yaw0-default`, the acting Fighter's ribbon runs through the dead raider's cell to an open destination with its bracket.
- `gate/`: the zero-warning build log and the full `Project.` report at the head (397/397, 0 warnings).
