# Project editor authoring

Use this guidance when adding or changing project assets, or verifying presentation and input in Unreal. Paths below are relative to the project checkout. The repository documents remain authoritative; this skill does not grant ticket or editor ownership.

1. Read `AGENTS.md` and `docs/development.md` before editor operations. For an upstream Unreal skill or connection recovery, also read `docs/agents/unreal-tools.md`.
2. Read the approved ticket, `docs/assets.md`, and `docs/architecture.md`. For gameplay changes, load the domain/spec context named by `AGENTS.md`. Reuse existing assets and declare any shared binary paths in the issue claim before editing.
3. Establish the editor owner and verify the exact project path, PID, and pinned engine through the helper. Discover relevant bundled skills and tools, then use the returned schemas for sequential calls.
4. Follow the task's recipe in `docs/editor-recipes.md`. Create missing output under the configured project content root, validate existing output, and preserve intentional edits. Record imported material and its license in `docs/assets.md`.
5. Save intentionally and verify individual outcomes plus complete dirty state. Reopen/reload as appropriate. Presentation/input claims require real interaction and inspected rendered output; a saved asset, headless test, or capture file's existence alone is insufficient.
6. Attach selected evidence another contributor can access, identifying tested source state, host/engine, commands, results, and limitations. Keep bulk output in ignored directories. Resume delivery through `docs/agents/workflow.md`.

Completion: the approved behavior is demonstrated, asset provenance and ownership are clear, existing work is preserved, and evidence accurately states what was observed.
