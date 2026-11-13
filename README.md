# Evidence for #218 PR A (head c2737568, base staging 6ee589d)

- `balance/base/<measurement>`: the five `Balance.Baseline.` measurements at contract 13's starting Honed values (step 0): `before` (revision 9, gear), and revision 10 `gear`, `reserve`, `honed`, `mixed`. Raw runs/encounters/heroes CSVs, `group.json` and `summary.*`.
- `balance/step1/...`: the kept lever step (every Honed row at its contract bound), the head's rules code. Its `before`, `gear` and `reserve` CSVs are byte-identical to step 0's.
- `balance/<step>/targets/targets.{json,md}`: `tools/balance_report.py targets` (P1-P6, the paired seed bootstrap, the holds).
- Comparisons (rerun with `tools/balance_report.py`):
  - `compare evidence/pr214-d249faf8:balance/final2 balance/step1/before --mode c1` passes.
  - `compare balance/step1/before balance/step1/gear --mode c2` passes.
  - `check-cards balance/step1/{reserve,honed,mixed} --gear balance/step1/gear` passes.
- `traces/rot5-04-20261116-7.txt`: the one deadlock under reserve and mixed (#217, accepted for the release on 2026-11-13).
- `gate/`: the zero-warning build log and the full `Project.` automation report at the head (359/359, 0 warnings).
- `audit/`: the contract 13 section 3 greps on the base (`*-base.txt`) and at the head (`*-head.txt`).
- `fixtures/schema5-capture-index.json`: the automation report of the temporary capture test that wrote `Tests/Fixtures/Schema5/` on unmodified 6ee589d.
