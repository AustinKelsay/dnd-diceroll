# Platform validation — 2026-10-30

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
