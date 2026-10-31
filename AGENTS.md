# Working in this project

Read `docs/development.md` before building, launching, editing assets, or using MCP. Read `docs/game-brief.md` before gameplay; undecided requirements need design input. Read `docs/architecture.md` for C++/Blueprint, pointer, property, and delegate conventions.

- Run `python3 tools/dev.py doctor` with Python 3.11+. `tools/versions.json` pins the engine; upgrades require build, bootstrap, automation, MCP/PIE, capture, and package validation.
- Use C++ foundations and Blueprint configuration, presentation, and appropriate behavior. Keep the existing runtime module until a dependency boundary warrants another.
- One agent owns the editor. Calls are sequential: `list_toolsets` → `describe_toolset` → `call_tool`. Verify the exact project path, listener PID, and engine before mutations; a map name or ProjectID is insufficient. The helper enforces identity for its MCP commands.
- On this Mac use `status` → `close` → `build` → `editor` for structural C++ changes, or `relaunch`. Live Coding is unsupported here. Refuse close when PIE or unsaved packages exist; save intentionally through Unreal and verify dirty state. Never force-quit an editor.
- Create binary assets through Unreal APIs/MCP and save explicitly. Work under `/Game/DnDDiceRoll`; record imports/licenses in `docs/assets.md`. Existing assets and class files must be preserved on rerun.
- Complete each slice with relevant build, automation, or editor evidence. Report exact checks and limitations. Generated evidence belongs in ignored `Saved/`, `.local/`, or `Builds/`.

## Agent skills

GitHub issues and PRs belong to `AustinKelsay/expedition-rpg`; read `docs/agents/issue-tracker.md` before tracker operations and `docs/agents/triage-labels.md` for state labels.

Read `CONTEXT.md`, `GLOSSARY.md`, and relevant `docs/adr/` before gameplay design, implementation, or review; `docs/agents/domain.md` defines the document layout. V1 behavior and testing seams are in `docs/specs/expedition-v1.md`; the active worker, commits, reviews, and editor owner are in `docs/runs/expedition-v1/ledger.md`.
