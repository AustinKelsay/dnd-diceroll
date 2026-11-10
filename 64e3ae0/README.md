# Evidence for PR #186 (#184, #112 slice 3 part 2), head `64e3ae0`

PlebDev Ultra, UE 5.8.4, `--cores 2`, machine lock. Same harness, policy version 1, 99 parties x 32 seeds.

- `before/`: the baseline at staging `bd02ede` (catalog revision 7): `summary.md`, `summary.json` and `baseline-csv.tar.gz` (every group's CSVs).
- `after/`: the same at this head (revision 8): `summary.md`, `summary.json`, `after-csv.tar.gz`. To recompute: extract into a folder under a checkout's `.local/` and run `python3 tools/balance_report.py <folder>`.
- `steps/`: target metrics (`targets.py`, a scratch helper, run on each folder) for the baseline, the four tried enemy-stat steps (step 3 kept) and the final after-run (identical to step 3).
- `automation/`: `verified.json` for full `Project.` (277/277, 0 warnings) and the after `Balance.Baseline.` (6/6, 0 warnings).
- `traces/long-fight-…`: the 50-round fight (`rot3-16`, seed 20261137, pattern 28, fight 5): a rescue loop under archer fire, a policy artifact (report, "What the tuned baseline now shows").
