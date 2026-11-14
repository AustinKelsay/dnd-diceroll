# Platform validation

## PlebDev Ultra (Linux) — 2026-11-07, issue #125

The validated host. Omarchy (Arch Linux) kernel 7.2.8-9-omarchy, x86_64; UE 5.8.4 CL 58902052 at `~/Applications/UnrealEngine-5.8.4` with its bundled Clang 20.1.8; NVIDIA RTX 4080 SUPER, driver 615.78.08, Vulkan; Python 3.14.8; Git LFS 3.8.1. Every command ran through `python3 tools/dev.py --cores 2 …` during an agreed turn on Ultra. Tested source: `c82f066` for build, tests, bootstrap, the skill check, smoke and capture; `cc0446b` for the later `close`, editor relaunch and `package` runs. Commits after it change only `doctor`'s informational GPU probe and docs.

| Check | Observed result |
| --- | --- |
| `doctor` | Host, `git`/`ps`/`lsof`, engine pin, plugins (AndroidFileServer disabled), bundled Clang, Git LFS and MCP configs pass |
| Helper tests | 60 pass |
| `build` | Linux Development editor target, up to 2 actions, exit 0, zero compiler or build-tool warning lines (the helper fails on any) |
| `test --filter Project.` | 99 executed, 99 passed, 0 failed/not run; zero warning or error lines; engine exit 0 via `SoftQuit`. The 48 expected checkpoint-fixture messages were matched by their per-test declarations |
| `bootstrap` | Existing sandbox validated, `0 error(s), 0 warning(s)`; every `Content/DnDDiceRoll` file byte-identical afterwards |
| `verify-skills` | Project skill found and read back; project, engine and instructions hash match; zero warnings |
| `editor` → `status` → `smoke` → `capture` → `close` | Status verified PID, listener, engine and project, no unsaved packages, no PIE. Smoke observed PIE running then stopped with no new errors or warnings. Capture: 2038×1270 PNG, inspected, showing the lit sandbox floor and PlayerStart. Close sent one SIGTERM; the log shows `RequestExit(bForce=false, ReturnCode=143)` and the PID exited |
| Relaunch right after `close` | Linux leaves the MCP port in TIME_WAIT for about a minute, and Unreal's listener binds without address reuse. `editor` waited about 60 s for the port, then started with MCP and closed normally |
| `package` | Linux Development BuildCookRun succeeded (compile 2 actions, cook `-corelimit=2` with one shader worker); the packaged game ran offscreen on Vulkan with 2 cores, entered `/Game/DnDDiceRoll/Maps/L_AgentSandbox`, and exited normally on one SIGTERM (code 143); zero warning or error lines in package and launch logs |
| Tracked config | Every file under `Config/` byte-identical before and after all runs |

Limits: the packaged game ran offscreen, so this proves startup, not on-screen rendering, input or performance. Smoke and capture use the sandbox, so they prove the helper, not gameplay. Two short UnrealPak staging steps (about 1 s each) ignore the core cap. Evidence stays in ignored `.local/` and `Builds/` in the validating checkout.

## Amaterasu (Windows) - 2026-11-13, issue #51

Advisory Windows host. This ladder does not replace PlebDev Ultra as the merge gate. Windows 11 Home 10.0.26200, AMD64; UE 5.8.4 CL 58902052 at the Epic default `UE_5.8` install; NVIDIA GeForce RTX 5080, driver 32.0.16.1692, DirectX 12 feature level 12_2 (AMD Radeon Graphics also present); Python 3.12.10; Git 2.55.0.windows.3; Git LFS 3.7.1. Every command ran through `py -3 tools/dev.py`, with `--cores 4` on build, test, bootstrap, editor, and package. Tested source: `c84e80cc` (`origin/staging` at the time of the run). Live Coding stayed off. The MCP endpoint and one-editor rule were unchanged. Tracked files under `Config/` stayed byte-identical. Helper still prints that off-Ultra results do not count as merge verification.

| Check | Observed result |
| --- | --- |
| `doctor` | Host, `git` / `powershell.exe` / `dxdiag.exe`, engine pin 5.8.4, editor plugins (AndroidFileServer disabled), DirectX 12 probe, Git LFS and MCP configs pass. Prints the off-Ultra NOTE and labels the host `(not validated)` because Ultra remains the merge gate. |
| `build` | Win64 Development editor target, up to 4 actions, exit 0, zero compiler or build-tool warning lines (12 actions, 47.65 s). Evidence: `.local/build/20261114T001756729111Z.log`, `.local/build.json` |
| `bootstrap` | Existing sandbox validated, zero errors or warnings. Evidence: `.local/bootstrap/20261114T001845262253Z.log` |
| `test --filter Project.Expedition.` | 368 executed, 368 passed, 0 failed. Engine exit 0 via `SoftQuit` (duration 110.72 s). The zero-warning gate failed on two `LogHttp` lines from the UE 5.8 editor home-screen probe (`https://www.google.com/generate_204`, 3.00 s timeout) landing on `Project.Expedition.Presentation.VaultKeyboardPanStaysInsideTheSet` (`succeededWithWarnings` 1). No other warning lines. This is issue #222 on current staging; PR #233 disables the probe. Evidence: `.local/automation/20261114T001853948075Z` |
| `test --filter Project.` | 381 executed, 381 passed, 0 failed. Engine exit 0 (duration 118.61 s). Same two #222 probe warning lines, same test, no other warnings. Evidence: `.local/automation/20261114T002104460782Z` |
| Optional #222 confirm | The same host, earlier the same day, ran full `Project.` twice on `7dff121f` (PR #233) and got 381 passed, 0 failed, 0 warning lines, 0 probe hits. That override is not in `c84e80cc`. |
| `editor` then `status` | Editor ready PID 26696, startup map `/Game/DnDDiceRoll/Maps/L_AgentSandbox`. Status verified that PID, the MCP listener, engine `5.8.4-58902052+++UE5+Release-5.8`, and the canonical `.uproject` path; no unsaved packages; PIE not active. Evidence: `.local/editor.json`, `.local/status.json` |
| `smoke` | PIE started, was observed running, and stopped; no new errors or warnings; packages stayed clean. Evidence: `.local/smoke-result.json` |
| `capture` | 1520x826 PNG, inspected, showing the lit sandbox floor and PlayerStart. Evidence: `.local/captures/20261114T002705205998Z.png` |
| `close` | Normal quit of the verified editor; PID 26696 exited normally. Evidence: `.local/51-validation/close-stdout.log` |
| `package` | Win64 Development BuildCookRun succeeded (compile 4-action cap, cook `-corelimit=4`; AutomationTool 69 s, exit 0). The packaged game launched windowed 1280x720, entered `/Game/DnDDiceRoll/Maps/L_AgentSandbox`, and exited normally (code 0, `LogExit: Exiting.`); zero warning or error lines in package and launch logs. Evidence: `Builds/20261114T002756925580Z/verified.json`, `package.log`, `launch.log` |

Limits: the packaged game run proves startup and a normal exit, not gameplay, input, or performance. Smoke and capture use the sandbox, so they prove the helper, not expedition play. Windows `perf` launches remain unsupported. The staging automation gate still fails on the #222 probe until PR #233 (or equivalent) lands. `doctor` and `check_host` still describe Windows as not the merge-gate host. Evidence stays in ignored `.local/` and `Builds/` in the validating checkout.

## Original Apple Silicon Mac validation — 2026-10-30 (historical)

The Mac is no longer a build or test host. This record describes the starter's first validation.

Validated on Apple Silicon macOS 26.5.2, Xcode 26.6, UE 5.8.4 CL 58902052. Host commands used `/opt/homebrew/bin/python3` (3.15.0); editor scripts ran in Unreal's Python 3.11.8.

| Check | Observed result | Local evidence |
| --- | --- | --- |
| Doctor | Host tools, engine pin, editor plugins, endpoint configs pass | `.local/doctor.log` |
| Helper tests | 22 stdlib tests pass, including ownership, dirty/PIE refusal, lock contention, protocol parsing, stale evidence, automation reports, template overwrite refusal, and build fingerprints | `.local/tooling-tests.log` |
| Editor compilation | Development editor target builds; generated component template also compiled, then temporary sample files were removed | `.local/platform-build.log`, `.local/platform-build-final.log`, `.local/build.json` |
| Running-editor guard | Build refused the open project before invoking UnrealBuildTool; unrelated blank editor excluded | Session output; helper tests |
| Existing sandbox | Clean bootstrap summary; map SHA-256 unchanged | `.local/bootstrap/20261030T194919187985Z.log`, `.local/bootstrap-before.json` |
| Bootstrap fixtures | First creation, unchanged second run, deliberate incomplete-map rejection with file preserved | `.local/bootstrap-fixtures/20261030T194012137730Z/.local/verified.json` |
| Unreal automation | `Project.Starter.SandboxAssets`: one passed, zero warnings/errors, after final config change | `.local/automation/20261030T194946382917Z/verified.json` and `index.json` |
| Zero-match automation | `Project.Starter.DoesNotExist` exits nonzero; missing/empty report cannot count as success | `.local/automation/20261030T194142826283Z/automation.log` |
| MCP | Live process/listener + canonical path + engine identity agree; toolset discovery and schemas succeed | `.local/status.json`, `.local/mcp-schemas/5.8.4/` |
| PIE | Exact sandbox floor found; play starts, is observed running, and stops; no new errors | `.local/smoke-result.json` |
| Viewport | 2027×1090 PNG inspected: lit floor visible against the minimal black background | `.local/captures/20261030T194404857202Z.png` |
| Safe close | Normal quit of the verified project editor succeeds, including full dirty-package gate after toolset registration; separate project browser untouched | Session output; helper tests |
| Mac package | Development build/cook/stage/package/archive succeeds; archived app enters configured sandbox with zero startup errors/warnings using null rendering | `Builds/20261030T194748720067Z/verified.json`, `package.log`, `launch.log` |

The first package launch exposed a missing GameFeatureData class from an editor-tool asset scan. Marking that scan editor-only fixed it; the succeeding packaged launch is the evidence above. Packaged startup captures full stdout because the initial app did not create the requested external log file. The automation report reader handles Unreal's UTF-8 BOM.

PIE recorded a macOS audio-device sample-rate warning (`LogAudioMixerAudioUnit`). It is retained in the smoke report and does not constitute a runtime error. No external assets were imported; the working map was preserved.

This establishes the Mac tooling workflow and minimal sandbox. Packaged rendering/input, gameplay, performance, Shipping signing/notarization, alternate engine locations, Finder-launched editor identity, visible save-dialog handling, other hosts, and other platforms remain untested. Dirty-state rejection and cleanup-failure behavior have offline coverage; no unrelated user work was saved or discarded to exercise those cases.

Reports and archives are ignored local evidence and are regenerated on another checkout. The editor is reopened at the end of validation; use `status` for current live state rather than treating this record as proof that it remains open.
