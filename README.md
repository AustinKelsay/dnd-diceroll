# Evidence for #241 M2 (PR #264, head 7a70c388, base staging 91fd28b0)

Host: PlebDev Ultra (Linux), UE 5.8.4 (CL 58902052). Every Unreal job ran under the machine lock at `--cores 2`.

- `balance/`: one folder per measurement, each with `summary.md`/`summary.json` (`tools/balance_report.py`) and each group's `group.json`. The raw CSVs stayed in `.local/`.
  - `m2-r10-<strategy>`, `m2-r11base-<strategy>`, `m2-r9-gear`, `m2-r8-gear` and `m2-r7-gear`: the references, on #240's head `50f5dfc2` (unmodified).
  - `m2-final-<strategy>`: revision 11 at the kept step (the PR's constants). `m2-final-rescues/mixed-rescues.csv` holds its re-down counts.
  - `m2-g2-*`: revisions 7–10 on this branch. `g2-identity.txt` shows that all 126 CSVs (`runs`, `encounters`, `heroes`, 6 groups × 7 measurements) are byte-identical to the references.
  - `x-*`: tuning exploration (`mixed` unless named), from a temporary build that read lever values from the environment. `x-off-mixed` is the instrumented baseline: CSVs byte-identical to `m2-r11base-mixed`. `x-hrpb-*` is byte-identical to `m2-final-*`.
  - `steps-quick.txt`: one line per measurement (`scripts/quick.py`). `m2-final-analysis.txt` and `final.json` hold the targets (`scripts/m2_analysis.py --ref m2-r10 --step m2-final`); `r11base.json` and `m2-r11base-analysis.txt` hold the baseline.
- `pin-digests-50f5dfc2.txt`: the six revision 7–9 digests `Project.Balance.Revisions7To9RunsArePinned` pins. They were captured on `50f5dfc2` plus the test file alone, and the final gate printed the same values.
- `gate/`: the zero-warning build log and the full `Project.` report at the head's source (413/413, 0 warnings).
- `scripts/`: the sweep, exploration and analysis scripts.
