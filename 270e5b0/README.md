# Evidence for PR #180 (#179, #112 slice 3), head `270e5b0`

PlebDev Ultra, UE 5.8.4, `--cores 2`, machine lock. Supersedes `evidence/pr180-0806944` (review round 1 fixes).

- `baseline/summary.md`, `summary.json`: `tools/balance_report.py` output for the full `Balance.Baseline.` run at this head (3,168 runs). `group-*.json`: per-group metadata.
- `baseline/baseline-csv.tar.gz`: **every** group's `runs.csv`, `encounters.csv`, `heroes.csv` and `group.json`. To recompute the report: extract it into a folder under a checkout's `.local/` (for example `.local/balance/check/`) and run `python3 tools/balance_report.py .local/balance/check`.
- `automation/`: the helper's `verified.json` for `test --filter Project.` (267/267, 0 warnings) and `test --filter Balance.Baseline.` (6/6, 0 warnings) at this head.
- `traces/stall-before-fix-…`: the first policy's round-60 stall (recorded before the fix; it does not reproduce now). `traces/death-…`: a rescue-deadline death on the final policy.
- `crash-1765717/`: the one shutdown crash of this work (2026-11-10 05:42Z, an early `Project.Balance.` run). `test-results.txt` shows that run had a **failing test** (`PolicyNeverPassesALegalAction`, the stall above), and `stack.txt` is the #157 signature (`Index >= 0` assert in `FVirtualTextureTileOffsetData::GetTileOffset` ← `FVirtualTextureSystem::DestroyPendingVirtualTextures` ← `~FVirtualTextureSystem`). The core is at `/var/lib/systemd/coredump/core.UnrealEditor-Cm.1000.b30b23bd566a48939617e0ecbc15d945.1765717.1794289347000000.zst` on Ultra.

The runs, encounters and heroes results are byte-identical across separate engine runs: at `0806944` (before the review fixes, before and after merging staging `d73b8ba`), and at `cec92f1` and `270e5b0` (after); only the blocker export changed format.
