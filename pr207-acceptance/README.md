# #152 integrated acceptance evidence (PR #207)

`staging` `f09abfe0fdae499db7a31511c43dc07229de8e11` (the #205 merge; #206 merged just before it), plus the documentation and Art-record edits of PR #207 (head `e8778e20` for the gallery runs after the first two; the commit differs from `f09abfe` only in `docs/` and three `Art/` files, so the native results are for `f09abfe`). PlebDev Ultra, UE 5.8.4, every Unreal job under the machine lock with `--cores 2`; perf presented to a private nested headless Hyprland.

- `logs/`: `build.log`, `test-project.log` (332/332), `test-expedition.log` (323/323), `test-fixtures.log` (1/1), `package.log` and `package-verified.json`.
- `perf/runA|runB|runC`: the report (`report.md`, `report.json`), each pass's `pass.json` and the helper logs of three complete default runs (separate locked warm-up, measured and resume passes). `perf/longframes.txt` is every run's worst frames and any 3-frame cluster over 20 ms (script: `perf/longframes.py`).
- `gallery/`: `logs/` (one per gallery job with its totals), `indexes/` (every run's `index.json`). 132 runs, 2,708 pictures in 19 jobs; see the PR.
- `contrast/`: `palette_report.md/json` (all 70 builds, 15 default-zoom shots each) and `dup_report.md/json` (four fighters, paladins and rogues).
- `captures/`: a few 1920x1080 captures from the gallery (the default party on Vigil, Root and the Vault, the far Vault, the start screen, the overview, the cast batches 3 and 4, the foes at the Vault finale, and four rogues and paladins with their accents).
