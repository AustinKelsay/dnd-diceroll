# Platform validation

## PlebDev Ultra (Linux) — 2026-11-07, issue #125

The validated host. Omarchy (Arch Linux) kernel 7.2.8-9-omarchy, x86_64; UE 5.8.4 CL 58902052 at `~/Applications/UnrealEngine-5.8.4` with its bundled Clang 20.1.8; NVIDIA RTX 4080 SUPER, driver 615.78.08, Vulkan; Python 3.14.8; Git LFS 3.8.1. Every command ran through `python3 tools/dev.py --cores 2 …` during an agreed turn on Ultra.

| Check | Observed result |
| --- | --- |
| `doctor` | Host, engine pin, plugins (AndroidFileServer disabled), bundled Clang, Git LFS and MCP configs pass |
| Helper tests | 54 pass |
| `build` | Linux Development editor target, up to 2 actions, exit 0, no compiler warnings or errors (38 s) |
| `test --filter Project.` | 99 executed, 99 passed, 0 failed/not run; zero warning or error lines; engine exit 0 via `SoftQuit` (39 s). The 48 expected checkpoint-fixture messages were matched by their per-test declarations |
| `editor` → `status` → `close` | Ready on the sandbox map in 11 s; status verified PID, listener, engine and project, no dirty packages, no PIE. Close sent one SIGTERM; the log shows `RequestExit(bForce=false, ReturnCode=143)`, normal teardown, and the process and port were gone in 5 s |
| `package` | Linux Development BuildCookRun succeeded (cook `-corelimit=2`, one shader worker); the packaged game rendered offscreen on Vulkan, entered `/Game/DnDDiceRoll/Maps/L_AgentSandbox`, and exited normally on one SIGTERM (code 143); zero warning or error lines in package and launch logs |
| Tracked config | `Config/DefaultEngine.ini` byte-identical before and after every engine run |

Limits: the packaged game ran offscreen, so this proves startup, not on-screen rendering, input or performance. The `smoke`, `capture`, `bootstrap` and `verify-skills` commands share the same Linux paths but were not re-run here. UAT's short global-container UnrealPak step does not accept a core cap. Evidence stays in ignored `.local/` and `Builds/` in the validating checkout.

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
