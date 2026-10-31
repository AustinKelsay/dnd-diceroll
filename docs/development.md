# Development environment

## Baseline and upgrades

`tools/versions.json` pins UE 5.8.4 (CL 58902052), validated on Apple Silicon macOS 26.5.2, Xcode 26.6, and Metal. The engine is installed at `/Users/Shared/Epic Games/UE_5.8`; `UE_ROOT` selects another installation. Latest stable at setup becomes a reproducible baseline. Check Epic's release notes before upgrading, then repeat build, bootstrap, automation, identity/discovery, PIE, capture, and packaged startup checks before changing the pin.

Unreal MCP is experimental and bundled with the engine. `ModelContextProtocol`, `AllToolsets`, Python scripting, and Editor Scripting Utilities are editor-only. No separate community MCP bridge or Python service is required. `tools/project.json` explicitly selects the GitHub repository the contribution checks expect, the module, editor/game targets, content root, and automation prefix. The helper discovers one root `.uproject` (or requires `--project` when ambiguous) and reads `EditorStartupMap` from config. Internal `DnDDiceRoll` IDs are retained for compatibility.

## Commands

Use Python **3.11+**, with no third-party dependencies. On this Mac `/opt/homebrew/bin/python3` selects the validated Homebrew interpreter; some shells resolve `python3` to Apple's 3.9. The helper prints its interpreter in `doctor` and rejects unsupported versions. Unreal's embedded Python is 3.11.8; keep `Content/Python` compatible with it and run those scripts only inside Unreal.

| Command (`python3 tools/dev.py …`) | Purpose |
| --- | --- |
| `doctor` | Check interpreter, host tools, pinned engine, and endpoint configurations |
| `verify-skills` | With this checkout closed, verify project-skill discovery and Markdown readback in a headless editor |
| `status` | Report associated processes, verified listener/project/build, map, PIE, and unsaved packages |
| `close` | Refuse PIE/dirty state; request normal quit of the verified editor PID and wait |
| `build` | Compile the Development editor target; refuse an open project |
| `bootstrap` | Create a missing sandbox, or validate and preserve an existing one |
| `editor` | Launch with explicit MCP start; wait for exact identity and startup map |
| `relaunch` | Close, build, and launch under one helper lock |
| `schemas EditorToolset.EditorAppToolset` | Save discovery/catalog and returned toolset schemas |
| `mcp` | Initialize a real session and list tools after identity verification |
| `smoke` | Require clean sandbox, verify exact floor label, start/observe/stop PIE, check new log errors |
| `capture` | Save a validated viewport PNG under `.local/captures/` |
| `test` | Run the configured project test prefix headlessly and validate its JSON report |
| `test --filter Project.Starter.SandboxAssets` | Run an explicit named test; zero matches fail |
| `test --filter Project.Tools.CaptureFixtures` | Write real Expedition checkpoints for each phase under `Saved/CaptureFixtures/<phase>/` for visual QA |
| `new-class component ExampleComponent` | Create a non-overwriting class in the configured module; also `actor` and `object` |
| `package` | Build/cook/stage/archive a Mac Development app, then verify headless startup to the default sandbox |

Run offline helper tests with `python3 -m unittest discover -s tools/tests -v`. `build`, `bootstrap`, `test`, and `package` refuse when this project is open, including when MCP is unavailable. Structural C++ changes use `status` → `close` → `build` → `editor` on this Mac. Live Coding is Win64 x64 in the installed engine; do not use it here.

The launcher refuses missing/stale build evidence and an occupied MCP port. A successful build records fingerprints of source, the project descriptor, module manifest, compiled binary, target, and pinned engine. Launch verifies that receipt rather than relying on timestamps; source deletions and no-action builds are handled correctly. It waits for readiness by default; `editor --no-wait` explicitly skips readiness. MCP auto-start is disabled so headless automation cannot claim the editor endpoint. Only the controlled launcher passes `-ModelContextProtocolStartServer`.

## Identity, ownership, and closing

One agent owns the running editor. The helper serializes all its operations with a nonblocking local lock; other clients and a human editor user must still coordinate. Calls execute sequentially. Verify identity before mutation: the endpoint listener PID must be this project's process under the pinned editor executable, and the project toolset must independently report that PID, the canonical `.uproject` path, and the exact engine build. Matching map names or copied ProjectIDs is insufficient. Finder launches can be associated using the exact open project log, but still require runtime confirmation. Saved launch metadata is advisory and never substitutes for live evidence.

`Content/Python/init_unreal.py` registers the small `unreal_starter.status.StarterWorkspaceTools` toolset. It reports both dirty content and dirty map packages, including unsaved packages. It creates no assets. It is pinned to the experimental UE registry API and registration safely does nothing when the registry is unavailable in a commandlet.

The same startup script imports the project authoring skill. [Unreal agent setup](agents/unreal-tools.md) covers optional Epic skills, compatibility rules, skill verification, and direct-connection recovery after editor restarts.

`close` refuses when PIE is active, packages are dirty, or identity cannot be proved. Save intentionally in Unreal, check individual save outcomes, and re-run status. The helper neither silently saves everything nor discards changes. It sends a normal macOS application quit to the exact PID and waits; if a save dialog appears or shutdown stalls, it fails without escalation. Never quit by app name when multiple editors exist. SIGTERM and `QUIT_EDITOR` bypass Unreal's normal save prompt and are unsuitable here.

For the one-time migration from an editor launched before the status tool existed, `close --request-quit` sends only the normal PID-targeted quit after OS identity checks. This explicit escape hatch skips the toolset dirty gate while retaining Unreal's save dialogs. Routine operations use `close`.

## Agent workflow and evidence

Use Matt Pocock skills directly under the [contribution workflow](agents/workflow.md), which owns planning, claims, review and staging delivery. Historical run reports retain their original launcher references.

Before extending the game, read the [current polish handoff](runs/expedition-polish/01-presentation-polish.md), inspect the branch and working tree, and establish editor ownership with fresh `status`. The [V1 ledger](runs/expedition-v1/ledger.md) is delivery history. Record the feature baseline, scope, owner, validation and unresolved limits in the current run record. Keep reviewed feature PRs targeting `staging`; merging remains a separate user decision.

Establish a slice and observable acceptance criteria in the game brief or a focused design note. Inspect the tree, run doctor, and use the safe compile loop for source changes. Discover `list_toolsets`, inspect `describe_toolset`, then dispatch `call_tool` using its schema. [Editor recipes](editor-recipes.md) maps common tasks to Epic's existing tools and skills. [Architecture](architecture.md) covers C++/Blueprint and ownership conventions.

Make bounded changes using stable paths, labels, and tags. Save assets explicitly. Preserve existing user work on reruns; fail clearly when existing recipe output is incomplete. `new_level()` can save an empty map before actor creation, so map existence alone is not completion. Bootstrap validates the floor, mesh/collision, lighting, and player start, checks spawn/save results for new output, and emits its completion marker only after success. A partial existing map requires explicit repair; bootstrap does not overwrite it.

Unreal automation is selected by a narrow prefix. The project test loads the configured map, verifies a unique floor with mesh/collision, a player start, and a valid packaged default-map package. The helper requires executed passing tests in `index.json`; process exit code alone is not proof. A zero-match run fails. The test uses null rendering; rendered appearance is established separately by an inspected viewport capture.

PIE evidence includes identity, build, timestamp, loaded map, observed running/stopped states, and new log errors/warnings. Log scanning begins at the operation's byte offset and matches Unreal severity fields, so historical errors and messages containing the word “Error” are not misclassified. Cleanup preserves the original failure; failed cleanup reports unknown PIE state. Timed-out mutations are not automatically replayed. Inspect failure evidence before acting again.

Generated output stays ignored: editor logs in `Saved/`, operation reports in `.local/`, packaged archives/logs in `Builds/`, and generated packaging templates/counters in `Build/Mac/`. Track platform resources deliberately when a product needs customization. Each automation/package run has a fresh directory. Packaging validates standalone startup using `-nullrhi`; it does not establish packaged rendering, input, performance, shipping signing/notarization, or another platform's SDK. A package failure leaves its logs for diagnosis. This is a Mac Development workflow, not a cross-platform abstraction.

The raw JSON-RPC escape hatch still performs exact identity verification:

```sh
python3 tools/dev.py mcp --method tools/call \
  --params '{"name":"list_toolsets","arguments":{}}'
```

Follow discovery before authoring calls. The helper sends explicit `captureTransform` and `annotations` to `CaptureViewport` because this pinned engine rejected their omission despite optional schema markings. Inspect installed plugin source when schema/runtime behavior disagrees.

## Source, assets, and connection troubleshooting

C++ lives in `Source/DnDDiceRoll`; project assets live in `Content/DnDDiceRoll`. Record imported assets/licenses in `docs/assets.md`. Git LFS tracks binary maps/assets and large art/audio. On a fresh clone, install Git LFS, run `git lfs install --local`, then `git lfs pull`. Engine binaries, caches, build output, and user settings remain untracked.

The endpoint is `http://127.0.0.1:18765/mcp`; port 8000 belongs to another service on this host. Start the editor before connecting a client. Restart existing agent sessions to load configuration. Codex project settings require trust; inspect `codex mcp list` from this directory. To change the endpoint, update `tools/versions.json`, `.codex/config.toml`, `.mcp.json`, and `DefaultEditorPerProjectUserSettings.ini` together. Keep it local: Epic's endpoint is intended for same-machine clients and has no authentication.

AllToolsets enables Game Features tooling transitively; the `GameFeatureData` scan satisfies its editor startup requirement and is marked editor-only so packaged games do not try to load the absent editor-tool dependency. Expedition has a fresh ProjectID and visible project name. Internal module/package identifiers remain compatible with the starter; renaming them requires a separate migration.

## Sources and validation records

The initial setup established editor compilation, commandlet bootstrap, MCP, PIE, and an inspected viewport. That historical evidence is under `.local/`; fresh platform validation is recorded in `docs/validation.md`. Do not infer a currently running editor from durable documentation.

- [Epic: UE 5.8.4 hotfix](https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775)
- [Epic: Unreal MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)
- [Epic: macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine)
- [Epic: automation test framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine)
- [Epic: packaging](https://dev.epicgames.com/documentation/unreal-engine/packaging-your-project)

Installed engine source was checked for configuration, registry APIs, quit behavior, Live Coding, test/report flags, and packaging arguments. See [council decisions](council-decisions.md) for review scope and video-attribution limits.
