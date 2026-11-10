# Evidence for PR #180 (#179, #112 slice 3)

Code at `7c32b52` (head `0806944` adds one report line). PlebDev Ultra, UE 5.8.4, `--cores 2`, machine lock.

- `baseline/summary.md`, `summary.json`: `tools/balance_report.py` output for the full `Balance.Baseline.` run (3,168 runs). `group-*.json`: per-group metadata (parties, seeds, policy version, harness seconds). `runs-Fixed.csv`: per-run rows of the nine representative parties. The rotation CSVs stay in ignored `.local/balance/`.
- `automation/`: the helper's `verified.json` for `test --filter Project.` (265/265, 0 warnings) and `test --filter Balance.Baseline.` (6/6, 0 warnings).
- `traces/stall-before-fix-…`: the first policy's round-60 stall (terrain-only approach jammed in a wall gap behind a body). Recorded before the fix, so it does not reproduce on the final policy.
- `traces/death-dup-cleric-…`: a rescue-deadline death on the final policy (the Downed cleric lies behind a wall; the designated rescuer arrives a turn late).

Byte-identical CSVs came from separate engine runs before and after merging staging `d73b8ba`.
