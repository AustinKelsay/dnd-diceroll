# Development environment

## Baseline and upgrades

`tools/versions.json` pins UE 5.8.4 (CL 58902052) and records the validated host, **PlebDev Ultra**: Linux x86_64 (Omarchy), the engine's bundled Clang, an NVIDIA GPU with Vulkan, and Python 3.11+. The engine is installed at `~/Applications/UnrealEngine-5.8.4`; `UE_ROOT` selects another installation. Ultra is the only host whose Unreal results count for merges. The helper keeps its old Mac paths, but Mac is no longer validated; Windows is tracked in #51. Latest stable at setup becomes a reproducible baseline. Check Epic's release notes before upgrading, then repeat build, bootstrap, automation, identity/discovery, PIE, capture, and packaged startup checks before changing the pin.

Unreal MCP is experimental and bundled with the engine. `ModelContextProtocol`, `AllToolsets`, Python scripting, and Editor Scripting Utilities are editor-only. No separate community MCP bridge or Python service is required. `tools/project.json` explicitly selects the GitHub repository the contribution checks expect, the module, editor/game targets, content root, and automation prefix. The helper discovers one root `.uproject` (or requires `--project` when ambiguous) and reads `EditorStartupMap` from config. Unreal module/package identifiers use `DnDDiceRoll`.

## Commands

Use Python **3.11+**, with no third-party dependencies; on Ultra `python3` is the system interpreter. The helper prints its interpreter in `doctor` and rejects unsupported versions. Unreal's embedded Python is 3.11.8; keep `Content/Python` compatible with it and run those scripts only inside Unreal.

| Command (`python3 tools/dev.py …`) | Purpose |
| --- | --- |
| `doctor` | Check interpreter, host tools (`git`, `ps`, `lsof`), pinned engine, and endpoint configurations; off Ultra it prints what to do instead |
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
| `test` | Run the configured project test prefix headlessly and validate its JSON report; any warning line fails (`--allow-warnings` only reports) |
| `test --filter Project.Starter.SandboxAssets` | Run an explicit named test; zero matches fail |
| `test --filter Project.Tools.CaptureFixtures` | Write real expedition checkpoints for each phase under `Saved/CaptureFixtures/<phase>/` for visual QA |
| `gallery [--fixture NAME ...] [--res WxH ...] [--primitives] [--poses PATH]` | Render the fixed-pose capture gallery under `.local/gallery/<timestamp>/` (see Capture gallery) |
| `new-class component ExampleComponent` | Create a non-overwriting class in the configured module; also `actor` and `object` |
| `package` | Build/cook/stage/archive a Linux Development game, start it offscreen until it enters the default map, then quit it normally |

Add `--cores 2` before the command on the shared machine (for example `python3 tools/dev.py --cores 2 build`). It caps compile actions, and engine cores for tests, bootstrap, the skill check, the editor, the cook, and the packaged game. Other commands refuse it. `build` fails on any compiler or build-tool warning. It only sees warnings from files it recompiles, so the merge gate builds each PR in a fresh checkout where every file compiles.

Run offline helper tests with `python3 -m unittest discover -s tools/tests -v`. `build`, `bootstrap`, `test`, and `package` refuse when this project is open, including when MCP is unavailable. Structural C++ changes use `status` → `close` → `build` → `editor`. Live Coding exists only for Win64 x64 in the installed engine; do not use it on Linux.

On Linux, `test` runs `UnrealEditor-Cmd` with `-RenderOffscreen` and `-ExecCmds="Automation RunTests <prefix>;SoftQuit"`. Do not add `-nullrhi`: it skips SDL setup and Slate then warns "Querying IsUsingWayland before SDL is initialized". Do not add `-TestExit`: on Unix it force-exits with code 1 even when tests pass. The helper also fails a build, test, bootstrap, skill check, or package run that changes any tracked file under `Config/`, even when the engine run itself failed; it reports the change and does not revert it.

The launcher refuses missing/stale build evidence and an occupied MCP port. A successful build records fingerprints of source, the project descriptor, module manifest, compiled binary, target, and pinned engine. Launch verifies that receipt rather than relying on timestamps; source deletions and no-action builds are handled correctly. It waits for readiness by default; `editor --no-wait` explicitly skips readiness. After an editor closes, Linux can hold the MCP port for about a minute (TIME_WAIT), and Unreal's listener cannot bind while it does. `editor` waits for the port to be free, and if Unreal still fails to bind it stops and tells you; close that editor from its window. MCP auto-start is disabled so headless automation cannot claim the editor endpoint. Only the controlled launcher passes `-ModelContextProtocolStartServer`.

## Identity, ownership, and closing

One agent owns the running editor. The helper serializes all its operations with a nonblocking local lock; other clients and a human editor user must still coordinate. Calls execute sequentially. Verify identity before mutation: the endpoint listener PID must be this project's process under the pinned editor executable, and the project toolset must independently report that PID, the canonical `.uproject` path, and the exact engine build. Matching map names or copied ProjectIDs is insufficient. An editor started outside the helper can be associated using the exact open project log, but still requires runtime confirmation. Saved launch metadata is advisory and never substitutes for live evidence.

`Content/Python/init_unreal.py` registers the small `unreal_starter.status.StarterWorkspaceTools` toolset. It reports both dirty content and dirty map packages, including unsaved packages. It creates no assets. It is pinned to the experimental UE registry API and registration safely does nothing when the registry is unavailable in a commandlet.

The same startup script imports the project authoring skill. [Unreal agent setup](agents/unreal-tools.md) covers optional Epic skills, compatibility rules, skill verification, and direct-connection recovery after editor restarts.

`close` refuses when PIE is active, packages are dirty, or identity cannot be proved. One small gap remains: a package could become dirty after the last check and before the quit request. On Linux no save dialog would catch it, so do not edit in the editor while `close` runs. Save intentionally in Unreal, check individual save outcomes, and re-run status. The helper neither silently saves everything nor discards changes.

On Linux, after those checks pass twice, `close` sends **one** SIGTERM to the verified editor PID and waits up to 120 seconds. Unreal treats the first SIGTERM as a normal exit request (exit code 143); a second signal forces exit, so the helper never repeats it and never uses SIGKILL. This path shows no save dialog, which is why the dirty and PIE checks are required. If the editor does not exit, the helper stops and reports it; check the editor yourself rather than signalling again. `close --request-quit` is refused on Linux. Never close an editor by name when several are running.

## Shared machine

Several agents use Ultra at once. Before any build, test, editor, or package job:

1. Check for running `UnrealEditor`, `UnrealEditor-Cmd`, `UnrealBuildTool`, `dotnet`, `ShaderCompileWorker`, or `RunUAT` processes (`ps -eo pid,args | grep -E 'Unreal|dotnet|ShaderCompile|RunUAT'`) and for a listener on port 18765.
2. Ask whoever coordinates Ultra (the maintainer, or the agent they named) for a turn to run Unreal, and wait until they say it is yours.
3. Run jobs one at a time from your own checkout with `--cores 2`. Close any editor you started with `close` and check that it exited.
4. When you are done, tell the coordinator, with your results, that Ultra is free again.

Never stop, signal, or force-quit another agent's process or editor, and never touch another checkout.

## Capture gallery

`python3 tools/dev.py --cores 2 gallery` renders a repeatable set of PNGs of the game itself, so later graphics slices compare against the same framings. It replaces driving a packaged window through the compositor: the game has a presentation-only capture mode and the helper only launches it.

1. Write fixtures once per checkout (and after changing fixture code): `python3 tools/dev.py --cores 2 test --filter Project.Tools.CaptureFixtures`. Besides the phase fixtures it writes `default-seed-route`, `default-seed-battle` (seed 20261030), `board-16x11` and `board-18x12` under `Saved/CaptureFixtures/<name>/`, each with a `fixture.json` naming the seed and board found (the 16x11 and 18x12 fixtures are the first seeds that generate those boards with the party acting first).
2. Build (`build`); the gallery refuses missing or stale build evidence, a missing fixture (it tells you to run step 1), a bad `--res`, bad poses, a running editor for this project and a held helper lock.
3. Run it. Defaults: fixtures `default-seed-battle`, `board-16x11`, `board-18x12`, `default-seed-route`; resolutions 1920x1080 and 1280x720; poses `tools/capture/poses.json`. `--primitives` renders the engine-primitive presentation; `--timeout` (default 300 s) bounds each game run, which gets one SIGTERM and is never killed.

Per fixture and resolution the helper copies the checkpoint into a fresh save root under the output folder (the real save and the fixture are never touched), launches `UnrealEditor <project> -game -windowed -ForceRes -ResX/-ResY -RenderOffscreen -UseFixedTimeStep -FPS=30 -ExpeditionSaveRoot -ExpeditionCapture -ExpeditionCaptureOut`, and verifies every shot: the PNG exists, is non-empty and has the requested size. Output: `.local/gallery/<timestamp>/<fixture>/<WxH>/<shot>.png`, a per-run `manifest.json` written by the game (shot, file, mode, yaw, distance, camera location/rotation/FOV, seed, board, themed flag, resolution), `game.log`, and `index.json` at the top (commit SHA and dirty flag, engine, fixture, seed, board, resolution, flags, shot list).

`tools/capture/poses.json` is the poses data (version 1): four yaws (-90, 0, 90, 180) times near (1300), default (the board's framed distance, 1720 + 70 per cell past 8x7) and far (3000) battle shots, two targeting shots, an `overview` shot and the `start-screen` shot. Distances must stay inside the battle clamp 900 to 3200. Modes: `battle`; `targeting` (Attack mode with the keyboard cursor on the nearest enemy in reach, or on the reachable step closest to one when none is in reach, so reach, path and target feedback show); `overview` (the route tabletop, needs a route-phase fixture); `start` (the title screen; always shot last). A shot that does not apply to a fixture (for example `overview` on a battle fixture, or `targeting` when an enemy holds the turn) is recorded as `skipped` with a reason and is not an error; a run with every shot skipped is. The game exits non-zero and writes a failed manifest on a missing checkpoint or a playback that never settles.

Capture mode only resumes and looks. It never submits a combat command (enemy turns are held) and never writes the checkpoint, and with `-ExpeditionCapture` absent the game is unchanged. The fixed time step and a fixed frame count after each pose make flame and idle motion repeat, and exposure adapts quickly (speed 20) so a shot does not depend on the one before it. Determinism limits: camera transforms repeat exactly; pixels repeat to within a mean absolute difference of about 0.03 of 255 on the same machine, GPU and driver (compare images with a tolerance, not byte equality). Results from another GPU or driver, or `-ForceRes` windows larger than the host desktop, are not promised to match. The enemy-turn view is not captured because it needs a played enemy turn, which capture mode must not submit.

## Agent workflow and evidence

Use Matt Pocock skills directly under the [contribution workflow](agents/workflow.md), which owns planning, claims, review and staging delivery. Historical run reports retain their original launcher references.

Before extending the game, read the [current state note](runs/current-state.md), inspect the branch and working tree, and establish editor ownership with fresh `status`. The [V1 ledger](runs/expedition-v1/ledger.md) and later run records are delivery history. Record the feature baseline, scope, owner, validation and unresolved limits in the current run record. Keep reviewed feature PRs targeting `staging`; merging remains a separate user decision.

Establish a slice and observable acceptance criteria in the game brief or a focused design note. Inspect the tree, run doctor, and use the safe compile loop for source changes. Discover `list_toolsets`, inspect `describe_toolset`, then dispatch `call_tool` using its schema. [Editor recipes](editor-recipes.md) maps common tasks to Epic's existing tools and skills. [Architecture](architecture.md) covers C++/Blueprint and ownership conventions.

Make bounded changes using stable paths, labels, and tags. Save assets explicitly. Preserve existing user work on reruns; fail clearly when existing recipe output is incomplete. `new_level()` can save an empty map before actor creation, so map existence alone is not completion. Bootstrap validates the floor, mesh/collision, lighting, and player start, checks spawn/save results for new output, and emits its completion marker only after success. A partial existing map requires explicit repair; bootstrap does not overwrite it.

Unreal automation is selected by a narrow prefix. The project test loads the configured map, verifies a unique floor with mesh/collision, a player start, and a valid packaged default-map package. The helper requires executed passing tests in `index.json`; process exit code alone is not proof. A zero-match run fails. Tests render offscreen; rendered appearance is established separately by an inspected viewport capture.

PIE evidence includes identity, build, timestamp, loaded map, observed running/stopped states, and new log errors/warnings. Log scanning begins at the operation's byte offset and matches Unreal severity fields, so historical errors and messages containing the word “Error” are not misclassified. Cleanup preserves the original failure; failed cleanup reports unknown PIE state. Timed-out mutations are not automatically replayed. Inspect failure evidence before acting again.

Generated output stays ignored: editor logs in `Saved/`, operation reports in `.local/`, and packaged archives/logs in `Builds/`. Track platform resources deliberately when a product needs customization. Each automation/package run has a fresh directory. Packaging validates offscreen standalone startup and a normal quit; it does not establish packaged rendering on screen, input, performance, or another platform's SDK. A package failure leaves its logs for diagnosis. This is a Linux Development workflow, not a cross-platform abstraction.

The raw JSON-RPC escape hatch still performs exact identity verification:

```sh
python3 tools/dev.py mcp --method tools/call \
  --params '{"name":"list_toolsets","arguments":{}}'
```

Follow discovery before authoring calls. The helper sends explicit `captureTransform` and `annotations` to `CaptureViewport` because this pinned engine rejected their omission despite optional schema markings. Inspect installed plugin source when schema/runtime behavior disagrees.

## Source, assets, and connection troubleshooting

C++ lives in `Source/DnDDiceRoll`; project assets live in `Content/DnDDiceRoll`. Record imported assets/licenses in `docs/assets.md`. Git LFS tracks binary maps/assets and large art/audio. On a fresh clone, install Git LFS, run `git lfs install --local`, then `git lfs pull`. Engine binaries, caches, build output, and user settings remain untracked.

The endpoint is `http://127.0.0.1:18765/mcp`; port 8000 belongs to another service on this host. Start the editor before connecting a client. Restart existing agent sessions to load configuration. Codex project settings require trust; inspect `codex mcp list` from this directory. To change the endpoint, update `tools/versions.json`, `.codex/config.toml`, `.mcp.json`, and `DefaultEditorPerProjectUserSettings.ini` together. Keep it local: Epic's endpoint is intended for same-machine clients and has no authentication.

AllToolsets enables Game Features tooling transitively; the `GameFeatureData` scan satisfies its editor startup requirement and is marked editor-only so packaged games do not try to load the absent editor-tool dependency. `dnd-diceroll` has its own ProjectID and visible project name. Internal module/package identifiers remain compatible with the starter; renaming them requires a separate migration.

## Sources and validation records

The initial setup established editor compilation, commandlet bootstrap, MCP, PIE, and an inspected viewport. That historical evidence is under `.local/`; fresh platform validation is recorded in `docs/validation.md`. Do not infer a currently running editor from durable documentation.

- [Epic: UE 5.8.4 hotfix](https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775)
- [Epic: Unreal MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)
- [Epic: Linux development requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/linux-development-requirements-for-unreal-engine)
- [Epic: automation test framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine)
- [Epic: packaging](https://dev.epicgames.com/documentation/unreal-engine/packaging-your-project)

Installed engine source was checked for configuration, registry APIs, quit behavior, Live Coding, test/report flags, and packaging arguments. See [council decisions](council-decisions.md) for review scope and video-attribution limits.
