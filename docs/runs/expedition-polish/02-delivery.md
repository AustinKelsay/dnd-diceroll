# Presentation polish delivery — 2026-10-31

Issue: [#26](https://github.com/AustinKelsay/expedition-rpg/issues/26). Scope: finish the existing presentation polish and shared-skills cleanup; preserve game rules, balance, save format, existing assets and RNG order. The user granted full-send authority, including reviewed staging integration. Main promotion and production deployment are outside this slice.

## Preservation and baseline

- Local polish preserved in `4c99d43`; current contribution factory integrated in `a6a9b71` from default-branch commit `4634325` (same source tree as staging `cdc37bf`). The README conflict was resolved by keeping both contributor onboarding and the current polish handoff.
- Existing feature branch retained. No source/binary work from another checkout was discarded. Root owns this checkout and editor coordination; the unrelated Unreal project browser is preserved.
- Shared skills: 21 custom source edits preserved on local archive branch `archive/local-skill-customizations-20261031`, commit `0706fe7`, plus a verified patch, Git bundle, source/store backups and hash manifest in the local `reconcile-shared-skills-20261031T193558Z` backup. No contribution was pushed to Matt's repository.
- Shared source is clean at upstream `4ae2317ddb22b046e26455e4f9c9b3aaaf62212d`. Canonical sync covers 39 upstream skills across nine stores, honoring the excluded exercise skill and Claude-only handoff. Same-name duplicate copies were archived before canonical sync. Custom unmanaged skills retain their hashes.
- Fresh factory `check`, `test:skills`, disabled `deploy:skills` and full `check:skills` pass. Root independently reran the full audit successfully. The prior blocked audit is resolved. Managed skill paths now live in configured stores such as `~/.codex/skills`; old session catalogs may need refreshing.

## Verification

Pinned host: Apple Silicon macOS, UE 5.8.4 / CL 58902052; Homebrew Python 3.15. Commands use `/opt/homebrew/bin/python3`.

- `tools/dev.py doctor`: passed, including direct MCP configuration consistency. Optional Epic recovery proxy remains unavailable and is not enabled.
- `python -m unittest discover -s tools/tests -v`: 50/50 passed after factory integration.
- `tools/contribute.py preflight` and `check`: passed after the deliberate merge completed.
- Fresh editor build: passed after integration, and again with the overlay regression/fix.
- Initial full `Project.` automation: 39/39 passed, zero warnings, `.local/automation/20261031T193635093071Z`.
- Bootstrap: validated/preserved sandbox, zero errors/warnings, `.local/bootstrap/20261031T193736820545Z.log`.
- `verify-skills`: project skill discovery/readback matched the exact checkout, source and engine, `.local/skills/20261031T193755043298Z`.
- Initial Mac package: built/cooked/archived and loaded the exact startup map, `Builds/20261031T193820392695Z`. The final package after the input fix also passed build/cook/archive and exact-map startup: `Builds/20261031T194438932989Z/verified.json`.

## Root regression

Help/Records blocked pointer clicks, but route/end-turn keyboard handlers could still mutate the hidden game. Added `Project.Expedition.Presentation.OverlaysBlockGameplayShortcuts` through the real player-controller handlers. Before the fix, its compiled run failed exactly three assertions: Help route choice, Help end turn, Records end turn (`.local/automation/20261031T194051623862Z`). After the fix, full `Project.` automation passes **40/40 with zero warnings** (`.local/automation/20261031T194337418509Z`); helper/contribution tests pass **50/50** again. The shared gameplay-input gate now covers target confirmation, route/reward/camp transitions, new/resume and end turn. Closing overlays restores input; the test also verifies that path.

## Rendered acceptance and runtime investigation

The final executable and packaged data were hash-matched against the final archive. Native CUA keyboard input used an isolated fixture in an owned sandbox directory; default player saves were untouched. Inspected captures include title, combat HUD, Help and overlay close under `.local/captures/polish/`. In the first process, Space while Help was open left the checkpoint bytes exactly unchanged. The process later exited unexpectedly: macOS report at 2026-10-31 14:50:49 CDT records an assertion path through `FSceneCullingBuilder::ProcessPostSceneUpdate` on a background worker (`.local/polish-runtime-crash.json`). There is no established project-level cause or claimed fix for that crash.

A fresh process with forced log flushing resumed the same checkpoint, completed three full Help → Space → Escape cycles with unchanged checkpoint bytes, remained rendered beyond the first process's lifetime, and then accepted Space with Help closed: the inspected HUD and committed checkpoint advanced from Nix to Ivo. Resume rebuilt the view again. This narrower reproduction did not reproduce the exit. The second process (PID 2426) then quit normally through Cmd-Q after about six minutes; its flushed log ends in normal shutdown with zero Error/Fatal lines (`.local/polish-render-repro2.log`). Fresh contribution preflight passes with zero commits behind staging and no project editor/listener. Runtime-crash adjudication remains open for independent review and owner investigation; a successful automation run is not a dismissal.

The reviewer independently verified UUID `1DCD926B-D350-3A01-9C01-829DD1B317B7` and mapped the original return address to `check(UploadDataProxy == nullptr)` in `TBufferScatterUploader::Reserve` (`RendererPrivateUtils.h:317`). This identifies the failed engine invariant, not its cause.

The requested next diagnostic was executed on the matching executable/checkpoint, PID 16957: LLDB attached successfully, resolved both `FDebug::CheckVerifyFailedImpl2` entry/island breakpoints, and installed an assertion callback to collect arguments/registers and all thread stacks. Five rendered Resume/rebuild cycles plus idle animation completed; breakpoints had **zero hits**. The process then exited normally with status **0**, with no Error/Fatal lines. Receipt: `.local/polish-debug-receipt.json`; flushed log: `.local/polish-debug-runtime.log`; command/capture harnesses remain ignored under `.local/`. No uploader address/task-order evidence could be captured without a recurrence. No baseline differential is claimed, because the symptom did not recur to compare. The runtime acceptance gap is still open.

## Independent review ledger

Round 1: fresh GPT-6.1 Sol on xhigh, fixed base `4634325` → `5efc2fc`, verdict **CHANGES**. Standards: no material source violations. Working-tree snapshots before/after both `39018be8f01455021b815e6ea6b1e2603bf34d72`; reviewer made no edits.

| ID | Severity | Outcome |
| --- | --- | --- |
| S1 | should | Rejected reward requests no longer finish playback. Extended the actual view regression; failed before fix. |
| S2 | should | Records pauses playback advancement and resumes the pending presentation on close. Enemy playback regression failed before fix. |
| S3 | should | Fallen enemies remain visible above their occupied cells; removed sinking/hiding while retaining fall, death tint and rules occupancy. A real-command victory/checkpoint/view regression failed for both enemy corpses before fix. |
| S4 | should | Restored the current seed in the existing status heading, without changing panel layout. Rendered recheck passed at 1280×760: the current seed is legible in the status heading. |
| A1 | blocker: acceptance | Runtime assertion remains open; binary UUID and assertion independently identified. Matching-executable debugger reproduction did not recur; no engine or speculative project workaround applied. |

Red evidence: `.local/automation/20261031T200942320818Z` (3 passed, 2 failed tests; loot/playback and both corpse assertions failed). Green: pinned editor build and full **41/41 Project automation, zero warnings** (`.local/automation/20261031T201248654038Z`), **50/50 helper/contribution tests** (`.local/polish-review-helpers.log`). Final corrected Mac Development package also passed build/cook/archive and exact-map startup (`Builds/20261031T201626746185Z/verified.json`). The launched executable and five packaged data files match that archive by SHA-256 (`.local/polish-final-package-hashes.json`). Inspected native final-package capture `.local/captures/polish/corrected-reward.png` at 1280×760 shows the restored seed and a fallen enemy body above the board outside the reward panel; automated geometry checks cover both slain enemies. The reward fixture resumed through native O input in a separate owned save directory. The exact final UI process (PID 28782) quit normally with zero Error/Fatal log lines (`.local/polish-final-runtime.log`); no owned editor or packaged app is retained. Round 2 independently marked S1–S4 resolved, found no material Standards violations, and retained A1 as a runtime acceptance blocker. Its new S5 finding identified an overbroad dead-state HUD guard making the existing fallen-party label branch unreachable. The minimal correction removes that extra dead-state filter; the existing branch continues to omit dead enemy labels and show fallen party identity. Fresh editor compilation and full **41/41 Project automation, zero warnings** passed (`.local/automation/20261031T202641721926Z`). Mac Development packaging and exact-map startup also passed (`Builds/20261031T202726893025Z/verified.json`); executable and five packaged data files are hash-matched to the native app (`.local/polish-s5-package-hashes.json`). The dead-party rendered recheck was attempted with a real defeat checkpoint and isolated save directory, but native PID 44985 has not reached Unreal startup: the read-only sample and LLDB inspection show dyld waiting in `open` for `Binaries/Mac/libtbb.12.dylib`. No game window or new runtime log exists, so this attempt does not establish the fallen label visually. The debugger was detached; normal CUA Cmd-Q was refused with `window_target_not_found`. No force quit or loader modification was performed. This owned pre-main process remains running and must be checked at handoff; it is separate from A1. Evidence: `.local/polish-fallen-startup-sample.txt`. Review tree before/after both `8615aad01d4aaf3a47f7382d8409b39ffe58835b`.

A1 disposition: the requested bounded debugger experiment is complete and did not reproduce the failure. Both reviewers agree the remaining risk needs explicit maintainer disposition in a concrete PR. No source fix or engine attribution is claimed. Sol-loop round 3 will check S5 and the remaining gaps; no unrelated code changes are planned.

## Limits

Mac Development remains the validated platform. Synthesized sound has not been judged by ear; timing is covered by playback automation and rendered observations, not an artistic feel approval. No existing binary assets, external imports, rule changes, cloud saves, concurrent-writer or power-loss guarantees are introduced.
