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
