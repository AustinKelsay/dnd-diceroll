# PR #193 gate and evidence at `9dee48de`

Verification of [PR #193](https://github.com/AustinKelsay/dnd-diceroll/pull/193) ("Fix missing Nanite usage flag on M_DiceRollTheme (#105)", CouchCoopGaming) at head **`9dee48de38773bd8f6d814b0e996cb6933eb6c1d`** (rebased onto `staging` `14e1384d`; the head's last commit is "re-apply Nanite usage on M_DiceRollTheme after #203 palettes").

Run on 2026-11-12 on PlebDev Ultra (Linux x86_64, i9-14900K, RTX 4080 SUPER 615.78.08, UE 5.8.4 CL 58902052) by an agent on the maintainer's behalf, in a fresh worktree of the head. Every Unreal job ran as `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock python3 tools/dev.py --cores 2 ...` (the asset inspections as `UnrealEditor-Cmd ... -corelimit=2` under the same lock), one at a time. Nothing was pushed to Landon's branch or to `staging`.

## Verdict

All gates pass at `9dee48de`. The resolved `M_DiceRollTheme` keeps staging's palette shader and adds the Nanite flag, and nothing else differs from staging's asset in the material graph. Open limits are at the end (no rendered SM6 pass on Ultra; staging has moved since the base).

## 1. Asset check (fresh headless `UnrealEditor-Cmd`, read-only, under the lock)

`/Game/DnDDiceRoll/Theme/Materials/M_DiceRollTheme`, loaded with each version in turn (the version swapped into the worktree for the run, restored after; `git status` clean afterwards). Scripts and raw results are in [`asset-check/`](asset-check) ([`summary.json`](asset-check/summary.json)).

| | HEAD `9dee48de` | staging `14e1384d` | `gate/193` `6539e123` |
| --- | --- | --- | --- |
| LFS object (sha256) | `3315e191…73cd` (95,128 B) | `c54ba6a4…2348` (95,079 B) | `8a732998…fdcb` (95,128 B; git blob `9ac4fdfa`) |
| **Nanite usage flag** (`has_material_usage`) | **true** | false | true |
| **Skeletal-mesh usage flag** | **true** | true | true |
| Graph revision tag `DiceRollGraphRevision` | **`dice-theme-7`** | `dice-theme-7` | `dice-theme-7` |
| Expressions / parameter names | 215 / 40 | 215 / 40 | 215 / 40 |
| Live palette Power node (reachable from the outputs) | **Base = Constant, Exp = Add (both wired)** | same | same |
| Material outputs connected | Base Color, Emissive, Opacity Mask, Roughness, Specular | same | same |
| Instances inheriting the master (`/Game/DnDDiceRoll`) | 96, all effective Nanite **true** | 96, all false | 96, all true |

- **Graph comparison.** A per-node signature (class, parameter name and default value, input classes, wired-input count; memory addresses stripped) is **identical for all three assets**: HEAD equals staging apart from the Nanite flag (the usage flag is the only difference found), and HEAD equals `gate/193` in everything compared. The two LFS objects differ (`3315e191…` vs `8a732998…`) as two separate saves of the same content, not as different graphs.
- **Palette Power node.** The master holds two Power nodes. The live one (feeds the five palette shift Divides that reach Base Color) has Base and Exp wired. The other is **unreachable from every material output** ([`head-reachability.json`](asset-check/head-reachability.json): 130 of 215 nodes reachable; the second Power node and 84 other nodes are dead leftovers) and has both pins unwired ([`head-power-nodes.json`](asset-check/head-power-nodes.json)). It is the same on staging (identical signature), so it predates this PR and does not affect the shader; it is dead weight in the graph and could be cleaned up on a later rebuild.
- Console logs of the three inspections: 0 warnings, 0 errors from the asset (`LogInit: Success - 0 error(s), 0 warning(s)`).

## 2. Gate at `9dee48de` ([`gate/`](gate))

| Check | Result |
| --- | --- |
| `python3 tools/dev.py --cores 2 build` (Development editor, fresh worktree so every project file compiled) | succeeded, **0 compiler warnings** |
| `test --filter Project.` (full automation) | **332 / 332 passed, 0 warnings**, 0 warning lines |
| Helper tests, `uv run --python 3.11 --no-project python -m unittest discover -s tools/tests` (Python 3.11.18) | **240 tests, OK** (235 on staging + the 5 new `test_nanite_usage.py` tests) |
| `python3 tools/contribute.py check` | **PASS** |
| `package` (Linux Development) | succeeded; the packaged game entered the default map and exited normally; **0 launch warnings, 0 package warnings, 0 package error lines** |
| `test --filter Project.Tools.CaptureFixtures` | 1 / 1, 0 warnings |

## 3. Performance ([`perf/report.md`](perf/report.md), [`report.json`](perf/report.json))

Packaged game from `9dee48de`, 1920x1080, `-novsync`, sound on, `--cores 2`, nested headless Hyprland of its own (`LIBSEAT_BACKEND=noop`, never the owner's display). One complete run `20261112T215526505937Z` (separate locked warm-up, measured and fresh-process resume passes, then the report); all three passes exit 0, `AUTOPLAY_DONE ok`, no SDL retry, no crash report.

| Metric | Measured | Overhaul budget | Result |
| --- | --- | --- | --- |
| p95 frame time | **3.43 ms** | 4.0 ms | pass |
| p95 GPU time | **2.69 ms** | 3.5 ms | pass |
| p95 game-thread time | **1.26 ms** | 1.5 ms | pass |
| Max RSS (VmHWM 1256.9 MiB) | **1261.0 MiB** | 1536 MiB (1.5 GiB) | pass |
| Steady-state max frame | 7.06 ms (25,193 frames) | n/a | |
| Worst transition hitches | route->battle 58.8 ms, resume 68.2 ms | hitch, reported | |

The helper's own (looser) limits also pass: "Performance gate: PASS". For context, the #152 acceptance on `staging` `f09abfe` measured 3.36 to 3.38 / 2.67 to 2.69 / 1.19 to 1.20 ms and 1243 to 1252 MiB over three runs: this head is within about 0.05 ms of the frame and 0.07 ms of game-thread numbers and 9 to 18 MiB of RSS. This is a single run here (the acceptance used three because of rare outliers).

**Log grep.**

- `usage flag` / `missing usage` / `Default Material will be used` / missing-material patterns: **0 matches** in the warm-up, measured and resume game logs and consoles, in the four gallery game logs, in both automation logs and in the asset-inspection logs.
- The measured and warm-up passes have 0 warning lines. The resume pass has 502 repeats of `LogExpeditionAutoplay: Warning: AUTOPLAY_WARN the themed route table never appeared; continuing without it`, the autoplay harness's own wait after the camp step. This is not new: the same line appears 481 to 500 times in the resume logs already on `evidence/release-20261110` (481), `evidence/onscreen-pr205-0b45942` (494) and `evidence/onscreen-pr199-53ba18e` (500).
- The editor-game gallery runs show the same 70 error and 21 warning lines each (engine plugin Python `ToolsetDefinition` and `LogEditorDataStorageUI` start-up diagnostics), none about materials; Landon's record of the same fixtures reports the same counts.

## 4. Capture gallery

`gallery --fixture default-seed-battle --fixture board-vigil-pillars --fixture board-root-r002 --fixture board-vault --res 1920x1080` after `test --filter Project.Tools.CaptureFixtures`: 4 runs, **172 pictures**, 16 shots skipped as not applicable, all exit 0, no crash. Fixture seeds: default-seed battle 20261030 (Root, the Stream trail), Vigil pillars seed 1, Root r002 seed 3, Vault seed 4. Downscaled 960x540 copies of the default-yaw battle shot on each (full-size captures stay in the local run folder):

| Default seed battle (Root) | Vigil | Root r002 | The Vault |
| --- | --- | --- | --- |
| ![default seed battle](captures/default-seed-battle-battle.png) | ![vigil](captures/vigil-board-battle.png) | ![root](captures/root-board-battle.png) | ![vault](captures/vault-board-battle.png) |

Title screen: [`captures/default-seed-battle-start-screen.png`](captures/default-seed-battle-start-screen.png).

## Limits

- **No rendered SM6 pass here.** Ultra runs `VULKAN_SM5`, where Nanite is off, so a missing Nanite flag cannot show up in these captures or logs; they establish that the head does not change SM5 rendering, loads cleanly and stays inside the budget. The SM6 claim rests on the saved asset's readback above (flag on the master, 96 of 96 instances effective true) and on Landon's Windows DX12/SM6 record (`docs/runs/nanite-usage/01-windows-sm6.md`), which describes an earlier asset and was not rerun at this head.
- **Staging has moved.** This head sits on `14e1384d`. `origin/staging` is now `cb25a414` (#185, Windows helpers: 28 files, Source/Tests/tools/docs, **no `.uasset` change**), so the PR does not conflict on the material, but the gate above is for `9dee48de` and not for a merge with `cb25a414`.
- One perf run only; the nested headless compositor does not represent a real display's present path.
- The first two jobs (build and the full `Project.` run) were started from a shell that inherited another session's `WAYLAND_DISPLAY`; both run offscreen and the perf and gallery jobs used the nested compositor of this run only.
- A diagnostic asset-inspection script of mine segfaulted the headless editor once (iterating invalid `MaterialProperty` entries) and left a crash report in this worktree's ignored `Saved/Crashes`. It is unrelated to the gate jobs and not in the logs above; the corrected script produced `head-reachability.json`.
