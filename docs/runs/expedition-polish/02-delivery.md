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

## Independent review ledger

Pending one fresh GPT-6.1 Sol review on xhigh, separately reporting Standards and Spec. Sol-loop budget: three rounds; later rounds only changed behavior/open findings. Snapshot the working tree before and after review, and leave scoped files unchanged while it runs.

## Limits

Mac Development remains the validated platform. Synthesized sound has not been judged by ear; timing is covered by playback automation and rendered observations, not an artistic feel approval. No existing binary assets, external imports, rule changes, cloud saves, concurrent-writer or power-loss guarantees are introduced.
