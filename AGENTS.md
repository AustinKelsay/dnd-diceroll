# Working in this project

For onboarding, Git updates, ticket work, PRs, or resuming a session, read `CONTRIBUTING.md`, then `docs/agents/workflow.md`. Keep the current checkout and editor owner intact; use an isolated workspace for simultaneous writing. For any other task, find its owning doc, template, or skill in `docs/agents/README.md`.

Use Matt Pocock's skills directly, with this repo's workflow supplying Unreal and contribution rules.

Read `docs/development.md` before building, launching, editing assets, or using MCP. Read `docs/game-brief.md` before gameplay; undecided requirements need design input. Read `docs/architecture.md` for C++/Blueprint, pointer, property, and delegate conventions.

For Epic skill installation, live editor tools, or connection recovery, read `docs/agents/unreal-tools.md` before following upstream defaults.

- Run `python3 tools/dev.py doctor` with Python 3.11+. `tools/versions.json` pins the engine; upgrades require build, bootstrap, automation, MCP/PIE, capture, and package validation.
- Use C++ foundations and Blueprint configuration, presentation, and appropriate behavior. Keep the existing runtime module until a dependency boundary warrants another.
- One agent owns the editor. Calls are sequential: `list_toolsets` → `describe_toolset` → `call_tool`. Verify the exact project path, listener PID, and engine before mutations; a map name or ProjectID is insufficient. The helper enforces identity for its MCP commands.
- On this Mac use `status` → `close` → `build` → `editor` for structural C++ changes, or `relaunch`. Live Coding is unsupported here. Refuse close when PIE or unsaved packages exist; save intentionally through Unreal and verify dirty state. Never force-quit an editor.
- Create binary assets through Unreal APIs/MCP and save explicitly. Work under `/Game/DnDDiceRoll`; record imports/licenses in `docs/assets.md`. Existing assets and class files must be preserved on rerun.
- Complete each slice with relevant build, automation, or editor evidence. Report exact checks and limitations. Generated evidence belongs in ignored `Saved/`, `.local/`, or `Builds/`.
